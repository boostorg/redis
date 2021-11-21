/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <chrono>

#include <aedis/net.hpp>
#include <aedis/resp3/request.hpp>

#include <boost/beast/core/stream_traits.hpp>
#include <boost/asio/yield.hpp>

namespace aedis {
namespace resp3 {

/** \file write.hpp
 *
 *  Write utility functions.
 */

template<class SyncWriteStream>
std::size_t
write(
   SyncWriteStream& stream,
   request& req,
   boost::system::error_code& ec)
{
    static_assert(boost::beast::is_sync_write_stream<SyncWriteStream>::value,
       "SyncWriteStream type requirements not met");

    return net::write(stream, net::buffer(req.payload), ec);
}

template<class SyncWriteStream>
std::size_t write(
   SyncWriteStream& stream,
   request& req)
{
    static_assert(boost::beast::is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream type requirements not met");

    boost::system::error_code ec;
    auto const bytes_transferred = write(stream, req, ec);

    if (ec)
        BOOST_THROW_EXCEPTION(boost::system::system_error{ec});

    return bytes_transferred;
}

template<
   class AsyncWriteStream,
   class Queue
>
struct write_some_op {
   AsyncWriteStream& stream;
   Queue& reqs;
   net::coroutine coro_ = net::coroutine();

   void
   operator()(
      auto& self,
      boost::system::error_code const& ec = {},
      std::size_t n = 0)
   {
      reenter (coro_) {
	 do {
	    assert(!std::empty(reqs));
	    assert(!std::empty(reqs.front().payload));

	    yield async_write(
	       stream,
	       net::buffer(reqs.front().payload),
	       std::move(self));

	    if (ec)
	       break;

            // Pops the request if no response is expected.
	    if (std::empty(reqs.front().commands))
	       reqs.pop();

	 } while (!std::empty(reqs) && std::empty(reqs.front().commands));

         self.complete(ec);
      }
   }
};

/** @brief Writes the some request from the queue in the stream.
 */
template<
  class AsyncWriteStream,
  class Queue,
  class CompletionToken =
      net::default_completion_token_t<typename AsyncWriteStream::executor_type>
  >
auto
async_write_some(
   AsyncWriteStream& stream,
   Queue& reqs,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncWriteStream::executor_type>{})
{
  return net::async_compose<
     CompletionToken,
     void(boost::system::error_code)>(
	write_some_op<AsyncWriteStream, Queue>{stream, reqs},
	token, stream);
}

template<
  class AsyncWriteStream,
  class CompletionToken =
      net::default_completion_token_t<typename AsyncWriteStream::executor_type>
  >
auto
async_write(
   AsyncWriteStream& stream,
   request const& req,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncWriteStream::executor_type>{})
{
   return net::async_write(stream, net::buffer(req.payload), token);
}

} // resp3
} // aedis

#include <boost/asio/unyield.hpp>
