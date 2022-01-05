/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/net.hpp>
#include <aedis/resp3/detail/write_ops.hpp>
#include <boost/beast/core/stream_traits.hpp>

namespace aedis {
namespace resp3 {

/** \ingroup read_write_ops
 *  @{
 */

/** @brief Writes requests from the queue to the stream.
  
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
     void(boost::system::error_code, std::size_t)>(
	detail::write_some_op<AsyncWriteStream, Queue>{stream, reqs},
	token, stream);
}

/*! @} */

} // resp3
} // aedis
