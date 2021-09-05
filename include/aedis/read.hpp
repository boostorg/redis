/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/net.hpp>
#include <aedis/type.hpp>
#include <aedis/pipeline.hpp>
#include <aedis/write.hpp>

#include <aedis/detail/parser.hpp>
#include <aedis/response_adapter_base.hpp>
#include <aedis/response_adapters.hpp>

namespace aedis {

response_adapter_base* select_adapter(response_adapters& buffers, resp3::type t, command cmd);

// The parser supports up to 5 levels of nested structures. The first
// element in the sizes stack is a sentinel and must be different from
// 1.
template <class AsyncReadStream, class Storage>
class parse_op {
private:
   AsyncReadStream& stream_;
   Storage* buf_ = nullptr;
   detail::parser parser_;
   int start_ = 1;

public:
   parse_op(AsyncReadStream& stream, Storage* buf, response_adapter_base* res)
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
            if (parser_.bulk() == detail::parser::bulk_type::none) {
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
   response_adapter_base& res,
   boost::system::error_code& ec)
{
   detail::parser p {&res};
   std::size_t n = 0;
   do {
      if (p.bulk() == detail::parser::bulk_type::none) {
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
   response_adapter_base& res)
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
auto async_read_one_impl(
   AsyncReadStream& stream,
   Storage& buffer,
   response_adapter_base& res,
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

public:
   type_op(AsyncReadStream& stream, Storage* buf)
   : stream_ {stream}
   , buf_ {buf}
   {
      assert(buf_);
   }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      if (ec) {
	 self.complete(ec, resp3::type::invalid);
         return;
      }

      if (std::empty(*buf_)) {
	 net::async_read_until(
	    stream_,
	    net::dynamic_buffer(*buf_),
	    "\r\n",
	    std::move(self));
	 return;
      }

      assert(!std::empty(*buf_));
      auto const type = resp3::to_type(buf_->front());
      // TODO: when type = resp3::type::invalid should we report an error or
      // complete normally and let the caller check whether it is invalid.
      self.complete(ec, type);
      return;
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
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code, resp3::type)
      >(type_op<AsyncReadStream, Storage> {stream, &buffer}, token, stream);
}

/** Asynchronously reads the response from one command. The result is
 *  stored in the parameter buffers.
 */
template < class AsyncReadWriteStream, class Storage>
net::awaitable<std::pair<command, resp3::type>>
async_read_one(
   AsyncReadWriteStream& socket,
   Storage& buffer,
   response_adapters& adapters,
   std::queue<pipeline> const& reqs)
{
   auto const type = co_await async_read_type(socket, buffer, net::use_awaitable);
   assert(type != resp3::type::invalid);

   auto cmd = command::unknown;
   if (type != resp3::type::push) {
      assert(!std::empty(reqs));
      assert(!std::empty(reqs.front().commands));
      cmd = reqs.front().commands.front();
   }

   auto* buf_adapter = select_adapter(adapters, type, cmd);
   co_await async_read_one_impl(socket, buffer, *buf_adapter, net::use_awaitable);
   co_return std::make_pair(cmd, type);
}

using transaction_queue_type = std::deque<std::pair<command, resp3::type>>;

// DEPRECATED
template < class AsyncReadWriteStream, class Storage>
net::awaitable<std::pair<command, resp3::type>>
async_consume(
   AsyncReadWriteStream& socket,
   Storage& buffer,
   response_buffers& bufs,
   std::queue<pipeline>& reqs)
{
   response_adapters adapters{bufs};
   auto const res = co_await async_read_one(socket, buffer, adapters, reqs);

   if (res.second == resp3::type::push)
      co_return res;

   reqs.front().commands.pop();
   // If that were that last command in the pipeline, delete the pipeline too.
   if (std::empty(reqs.front().commands)) {
      reqs.pop();
      // Now we should write any the next pipeline waiting in the
      // queue.  Notice that commands like unsubscribe have a push
      // response type so we do not have to wait for a response before
      // sending a new pipeline.
      while (!std::empty(reqs)) {
	 auto buffer = net::buffer(reqs.front().payload);
	 auto const n = co_await async_write(socket, buffer, net::use_awaitable);
	 if (!std::empty(reqs.front().commands))
	    break;

	 // We only pop when all commands in the pipeline has push
	 // responses like subscribe, otherwise, pop is done when the
	 // response arrives.
	 reqs.pop();
      }
   }

   co_return res;
}

template<class Receiver>
net::awaitable<void>
async_read(
   net::ip::tcp::socket& socket,
   std::string& buffer,
   response_buffers& buffers,
   std::queue<pipeline>& pipelines,
   Receiver receiver)
{
   for (;;) {
      co_await async_write_some(socket, pipelines, net::use_awaitable);

      do {
	 do {
	    response_adapters adapters{buffers};

	    auto const type =
	       co_await async_read_type(socket, buffer, net::use_awaitable);

	    auto cmd = command::unknown;
	    if (type != resp3::type::push) {
	       cmd = pipelines.front().commands.front();
	       pipelines.front().commands.pop();
	    }

	    auto* adapter = select_adapter(adapters, type, cmd);
	    co_await async_read_one_impl(socket, buffer, *adapter, net::use_awaitable);
	    receiver(cmd, type);

	 } while (!std::empty(pipelines.front().commands));
         pipelines.pop();
      } while (std::empty(pipelines));
   }
}

} // aedis
