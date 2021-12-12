/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <chrono>

#include <aedis/net.hpp>
#include <aedis/resp3/serializer.hpp>

#include <boost/beast/core/stream_traits.hpp>
#include <boost/asio/yield.hpp>

namespace aedis {
namespace resp3 {

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
	    assert(!std::empty(reqs.front().request()));

	    yield net::async_write(
	       stream,
	       net::buffer(reqs.front().request()),
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

} // resp3
} // aedis

#include <boost/asio/unyield.hpp>
