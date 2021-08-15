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
#include <aedis/type.hpp>
#include <aedis/pipeline.hpp>

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
   resp3::type* t_;

public:
   type_op(AsyncReadStream& stream, Storage* buf, resp3::type* t)
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
      *t_ = resp3::to_type(buf_->front());
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
   resp3::type& t,
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

// Returns true when a new pipeline can be sent to redis.
bool queue_pop(std::queue<pipeline>& reqs);

using transaction_queue_type = std::deque<std::pair<command, resp3::type>>;

// TODO: Implement as a composed operation.
template <class AsyncReadWriteStream, class Storage>
net::awaitable<transaction_queue_type>
async_read_transaction(
   AsyncReadWriteStream& socket,
   Storage& buffer,
   response_base& reader,
   std::queue<pipeline>& reqs,
   boost::system::error_code& ec)
{
   transaction_queue_type trans;
   for (;;) {
      auto const cmd = reqs.front().cmds.front();
      if (cmd != command::exec) {
         response_static_string<6> tmp;
         co_await async_read(socket, buffer, tmp, net::redirect_error(net::use_awaitable, ec));

         if (ec)
            co_return transaction_queue_type{};

         // Failing to QUEUE a command inside a trasaction is
         // considered an application error.  The multi command
         // always gets a "OK" response and all other commands get
         // QUEUED unless the user is e.g. using wrong data types.
         auto const* res = cmd == command::multi ? "OK" : "QUEUED";
         assert (tmp.result == res);

         // Pushes the command in the transction command queue that will be
         // processed when exec arrives.
         trans.push_back({reqs.front().cmds.front(), resp3::type::invalid});
         reqs.front().cmds.pop();
         continue;
      }

      if (cmd == command::exec) {
         assert(trans.front().first == command::multi);
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

// TODO: Handle errors properly.
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
   std::queue<pipeline>& reqs,
   boost::system::error_code& ec)
{
   for (;;) {
      auto type = resp3::type::invalid;
      co_await async_read_type(socket, buffer, type, net::redirect_error(net::use_awaitable, ec));

      if (ec)
         co_return;

      if (type == resp3::type::invalid) {
	 // TODO: Add our own error code.
	 assert(false);
      }

      if (type == resp3::type::push) {
         co_await async_read(
	    socket,
	    buffer,
	    resps.resp_push,
	    net::redirect_error(net::use_awaitable, ec));

         if (ec)
            co_return;

	 recv.on_push(resps.resp_push.result);
         continue;
      }

      assert(!std::empty(reqs));
      assert(!std::empty(reqs.front().cmds));

      if (reqs.front().cmds.front() == command::multi) {
         // The exec response is an array where each element is the
         // response of one command in the transaction. This requires
         // a special response buffer, that can deal with recursive
         // data types.
         auto const trans_queue =
	    co_await async_read_transaction(
	       socket,
	       buffer,
	       resps.resp_tree,
	       reqs,
	       ec);

         if (ec)
            co_return;

         forward_transaction(resps.resp_tree.result, trans_queue, recv);

         if (queue_pop(reqs))
	    co_await async_write_all(socket, reqs, ec);

         continue;
      }

      auto const cmd = reqs.front().cmds.front();
      auto* tmp = select_buffer(resps, type);
      co_await async_read(socket, buffer, *tmp, net::redirect_error(net::use_awaitable, ec));

      if (ec)
         co_return;

      forward(resps, cmd, type, recv);

      if (queue_pop(reqs))
	 co_await async_write_all(socket, reqs, ec);
   }
}

} // detail
} // aedis
