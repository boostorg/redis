/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <deque>
#include <string>
#include <cstdio>
#include <utility>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <iostream>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <string_view>
#include <charconv>

#include <aedis/net.hpp>
#include <aedis/types.hpp>
#include <aedis/request.hpp>

#include "parser.hpp"
#include "response_buffers.hpp"
#include "response_base.hpp"

namespace aedis { namespace detail {

// The parser supports up to 5 levels of nested structures. The first
// element in the sizes stack is a sentinel and must be different from
// 1.
template <class AsyncReadStream, class Storage>
class parse_op {
private:
   AsyncReadStream& stream_;
   Storage* buf_ = nullptr;
   parser parser_;
   int start_ = 1;

public:
   parse_op(AsyncReadStream& stream, Storage* buf, response_base* res)
   : stream_ {stream}
   , buf_ {buf}
   , parser_ {res}
   { }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      switch (start_) {
         for (;;) {
            if (parser_.bulk() == parser::bulk_type::none) {
               case 1:
               start_ = 0;
               net::async_read_until(
                  stream_,
                  net::dynamic_buffer(*buf_),
                  "\r\n",
                  std::move(self));

               return;
            }

	    // On a bulk read we can't read until delimiter since the
	    // payload may contain the delimiter itself so we have to
	    // read the whole chunk. However if the bulk blob is small
	    // enough it may be already on the buffer buf_ we read
	    // last time. If it is, there is no need of initiating
	    // another async op otherwise we have to read the
	    // missing bytes.
            if (std::ssize(*buf_) < (parser_.bulk_length() + 2)) {
               start_ = 0;
	       auto const s = std::ssize(*buf_);
	       auto const l = parser_.bulk_length();
	       auto const to_read = static_cast<std::size_t>(l + 2 - s);
               buf_->resize(l + 2);
               net::async_read(
                  stream_,
                  net::buffer(buf_->data() + s, to_read),
                  net::transfer_all(),
                  std::move(self));
               return;
            }

            default:
	    {
	       if (ec)
		  return self.complete(ec);

	       n = parser_.advance(buf_->data(), n);
	       buf_->erase(0, n);
	       if (parser_.done())
		  return self.complete({});
	    }
         }
      }
   }
};

template <class SyncReadStream, class Storage>
auto read(
   SyncReadStream& stream,
   Storage& buf,
   response_base& res,
   boost::system::error_code& ec)
{
   parser p {&res};
   std::size_t n = 0;
   do {
      if (p.bulk() == parser::bulk_type::none) {
	 n = net::read_until(stream, net::dynamic_buffer(buf), "\r\n", ec);
	 if (ec || n < 3)
	    return n;
      } else {
	 auto const s = std::ssize(buf);
	 auto const l = p.bulk_length();
	 if (s < (l + 2)) {
	    buf.resize(l + 2);
	    auto const to_read = static_cast<std::size_t>(l + 2 - s);
	    n = net::read(stream, net::buffer(buf.data() + s, to_read));
	    assert(n >= to_read);
	    if (ec)
	       return n;
	 }
      }

      n = p.advance(buf.data(), n);
      buf.erase(0, n);
   } while (!p.done());

   return n;
}

template<class SyncReadStream, class Storage>
std::size_t
read(
   SyncReadStream& stream,
   Storage& buf,
   response_base& res)
{
   boost::system::error_code ec;
   auto const n = read(stream, buf, res, ec);

   if (ec)
       BOOST_THROW_EXCEPTION(boost::system::system_error{ec});

   return n;
}

template <
   class AsyncReadStream,
   class Storage,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read(
   AsyncReadStream& stream,
   Storage& buffer,
   response_base& res,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(parse_op<AsyncReadStream, Storage> {stream, &buffer, &res},
        token,
        stream);
}

template <class AsyncReadStream, class Storage>
class type_op {
private:
   AsyncReadStream& stream_;
   Storage* buf_ = nullptr;
   types* t_;

public:
   type_op(AsyncReadStream& stream, Storage* buf, types* t)
   : stream_ {stream}
   , buf_ {buf}
   , t_ {t}
   {
      assert(buf_);
   }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      if (ec)
	 return self.complete(ec);

      if (std::empty(*buf_)) {
	 net::async_read_until(
	    stream_,
	    net::dynamic_buffer(*buf_),
	    "\r\n",
	    std::move(self));
	 return;
      }

      assert(!std::empty(*buf_));
      *t_ = to_type(buf_->front());
      return self.complete(ec);
   }
};

template <
   class AsyncReadStream,
   class Storage,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read_type(
   AsyncReadStream& stream,
   Storage& buffer,
   types& t,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(type_op<AsyncReadStream, Storage> {stream, &buffer, &t},
        token,
        stream);
}

struct queue_elem {
   request req;
   bool sent = false;
};

using request_queue = std::queue<queue_elem>;

// Returns true when a new request can be sent to redis.
bool queue_pop(request_queue& reqs);

using transaction_queue_type = std::deque<std::pair<commands, types>>;

// TODO: Implement as a composed operation.
template <class AsyncReadWriteStream, class Storage>
net::awaitable<transaction_queue_type>
async_read_transaction(
   AsyncReadWriteStream& socket,
   Storage& buffer,
   response_base& reader,
   request_queue& reqs,
   boost::system::error_code& ec)
{
   transaction_queue_type trans;
   for (;;) {
      auto const cmd = reqs.front().req.cmds.front();
      if (cmd != commands::exec) {
         response_static_string<6> tmp;
         co_await async_read(socket, buffer, tmp, net::redirect_error(net::use_awaitable, ec));

         if (ec)
            co_return transaction_queue_type{};

         // Failing to QUEUE a command inside a trasaction is
         // considered an application error.  The multi commands
         // always gets a "OK" response and all other commands get
         // QUEUED unless the user is e.g. using wrong data types.
         auto const* res = cmd == commands::multi ? "OK" : "QUEUED";
         assert (tmp.result == res);

         // Pushes the command in the transction command queue that will be
         // processed when exec arrives.
         trans.push_back({reqs.front().req.cmds.front(), types::invalid});
         reqs.front().req.cmds.pop();
         continue;
      }

      if (cmd == commands::exec) {
         assert(trans.front().first == commands::multi);
         co_await async_read(socket, buffer, reader, net::redirect_error(net::use_awaitable, ec));
         if (ec)
            co_return transaction_queue_type{};

         trans.pop_front(); // Removes multi.
         co_return trans;
      }
      assert(false);
   }

   co_return transaction_queue_type{};
}

template <
   class AsyncReadWriteStream,
   class Storage,
   class Receiver,
   class ResponseBuffers>
net::awaitable<void>
async_reader(
   AsyncReadWriteStream& socket,
   Storage& buffer,
   ResponseBuffers& resps,
   Receiver& recv,
   request_queue& reqs,
   boost::system::error_code& ec)
{
   for (;;) {
      auto t = types::invalid;
      co_await async_read_type(socket, buffer, t, net::redirect_error(net::use_awaitable, ec));

      if (ec)
         co_return;

      assert(t != types::invalid);

      if (t == types::push) {
         auto* tmp = resps.select(commands::unknown, types::push);

         co_await async_read(socket, buffer, *tmp, net::redirect_error(net::use_awaitable, ec));
         if (ec)
            co_return;

         resps.forward(commands::unknown, types::push, recv);
         continue;
      }

      assert(!std::empty(reqs));
      assert(!std::empty(reqs.front().req.cmds));

      if (reqs.front().req.cmds.front() == commands::multi) {
         // The exec response is an array where each element is the
         // response of one command in the transaction. This requires
         // a special response buffer, that can deal with recursive
         // data types.
         auto* reader = resps.select(commands::exec, types::invalid);
         auto const trans_queue =
            co_await async_read_transaction(socket, buffer, *reader, reqs, ec);

         if (ec)
            co_return;

         resps.forward_transaction(trans_queue, recv);

         if (queue_pop(reqs)) {
            // The following loop apears again in this function below. It
            // should to be implement as a composed operation.
            while (!std::empty(reqs) && !reqs.front().sent) {
               reqs.front().sent = true;
               auto buffer = net::buffer(reqs.front().req.payload);
               co_await async_write(
                  socket,
                  buffer,
                  net::redirect_error(net::use_awaitable, ec));

               if (ec) {
                  reqs.front().sent = false;
                  co_return;
               }

               if (!std::empty(reqs.front().req.cmds))
                  break;

               reqs.pop();
            }
         }

         continue;
      }

      auto const cmd = reqs.front().req.cmds.front();
      auto* tmp = resps.select(cmd, t);
      co_await async_read(socket, buffer, *tmp, net::redirect_error(net::use_awaitable, ec));

      if (ec)
         co_return;

      resps.forward(cmd, t, recv);

      if (queue_pop(reqs)) {
         // Commands like unsubscribe have a push response so we do not
         // have to wait for a response before sending a new request.
         while (!std::empty(reqs) && !reqs.front().sent) {
            reqs.front().sent = true;
            auto buffer = net::buffer(reqs.front().req.payload);
            co_await async_write(
               socket,
               buffer,
               net::redirect_error(net::use_awaitable, ec));

            if (ec) {
               reqs.front().sent = false;
               co_return;
            }

            if (!std::empty(reqs.front().req.cmds))
               break;

            reqs.pop();
         }
      }
   }
}

} // detail
} // aedis
