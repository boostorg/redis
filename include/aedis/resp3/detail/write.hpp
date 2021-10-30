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
namespace detail {

template<class SyncWriteStream>
std::size_t
write(
   SyncWriteStream& stream,
   request& req,
   boost::system::error_code& ec)
{
    static_assert(boost::beast::is_sync_write_stream<SyncWriteStream>::value,
       "SyncWriteStream type requirements not met");

    return write(stream, net::buffer(req.payload), ec);
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

/* Asynchronously writes one or more requests on the stream.
 */
template<class AsyncWriteStream>
struct write_some_op {
   AsyncWriteStream& stream;
   std::queue<request>& requests;
   net::coroutine coro = net::coroutine();

   void
   operator()(
      auto& self,
      boost::system::error_code const& ec = {},
      std::size_t n = 0)
   {
      reenter (coro) {
	 do {
	    assert(!std::empty(requests));
	    assert(!std::empty(requests.front().payload));

	    yield async_write(
	       stream,
	       net::buffer(requests.front().payload),
	       std::move(self));

	    if (ec)
	       break;

	    if (std::empty(requests.front().commands)) {
	       // We only pop when all commands in the pipeline have push
	       // responses like subscribe, otherwise, pop is done when the
	       // response arrives.
	       requests.pop();
	    }
	 } while (!std::empty(requests) && std::empty(requests.front().commands));

         self.complete(ec);
      }
   }
};

template<
  class AsyncWriteStream,
  class CompletionToken =
      net::default_completion_token_t<typename AsyncWriteStream::executor_type>
  >
auto
async_write_some(
   AsyncWriteStream& stream,
   std::queue<request>& requests,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncWriteStream::executor_type>{})
{
  return net::async_compose<
     CompletionToken,
     void(boost::system::error_code)>(
	write_some_op{stream, requests},
	token, stream);
}

} // detail
} // resp3
} // aedis

#include <boost/asio/unyield.hpp>
