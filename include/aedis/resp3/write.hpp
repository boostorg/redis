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
    \brief Write utility functions.
  
    Both synchronous and asynchronous functions are offered.
 */

/** @brief Writes a request to and stream.
 *
 *  \param stream Sync stream where the request will be written.
 *  \param req Sync stream where the request will be written.
 *  \param ec Variable where an error is written if one occurs.
 *  \returns The number of bytes that have been written to the stream.
 */
template<
   class SyncWriteStream,
   class Request
>
std::size_t
write(
   SyncWriteStream& stream,
   Request& req,
   boost::system::error_code& ec)
{
    static_assert(boost::beast::is_sync_write_stream<SyncWriteStream>::value,
       "SyncWriteStream type requirements not met");

    return net::write(stream, net::buffer(req.payload()), ec);
}

/** @brief Writes a request to and stream.
 *
 *  \param stream Sync stream where the request will be written.
 *  \param req Sync stream where the request will be written.
 *  \returns The number of bytes that have been written to the stream.
 */
template <
   class SyncWriteStream,
   class Request
>
std::size_t write(
   SyncWriteStream& stream,
   Request& req)
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
	    assert(!std::empty(reqs.front().payload()));

	    yield net::async_write(
	       stream,
	       net::buffer(reqs.front().payload()),
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

/** @brief Writes the request to the stream.
 */
template<
  class AsyncWriteStream,
  class Request,
  class CompletionToken =
      net::default_completion_token_t<typename AsyncWriteStream::executor_type>
  >
auto
async_write(
   AsyncWriteStream& stream,
   Request const& req,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncWriteStream::executor_type>{})
{
   return net::async_write(stream, net::buffer(req.payload()), token);
}

} // resp3
} // aedis

#include <boost/asio/unyield.hpp>
