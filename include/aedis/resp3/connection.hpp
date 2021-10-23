/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/net.hpp>

#include <aedis/resp3/request.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/response.hpp>
#include <aedis/resp3/detail/read.hpp>

namespace aedis {
namespace resp3 {

/** Reads and writes redis commands.
 */
struct connection {
   std::string buffer;
   net::coroutine coro = net::coroutine();
   type t = type::invalid;

   template<
      class AsyncReadWriteStream,
      class CompletionToken =
	 net::default_completion_token_t<typename AsyncReadWriteStream::executor_type>
   >
   auto async_consume(
      AsyncReadWriteStream& stream,
      std::queue<request>& requests,
      response& resp,
      CompletionToken&& token =
         net::default_completion_token_t<typename AsyncReadWriteStream::executor_type>{})
   {
     return net::async_compose<
	CompletionToken,
	void(boost::system::error_code, type)>(
	   detail::consumer_op{stream, buffer, requests, resp, t, coro},
	   token, stream);
   }
};

} // resp3
} // aedis
