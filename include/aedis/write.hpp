/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <chrono>

#include <aedis/net.hpp>
#include <aedis/pipeline.hpp>

#include <boost/beast/core/stream_traits.hpp>

namespace aedis {

bool prepare_queue(std::queue<pipeline>& reqs);

template<class SyncWriteStream>
std::size_t
write(
   SyncWriteStream& stream,
   pipeline& req,
   boost::system::error_code& ec)
{
    static_assert(boost::beast::is_sync_write_stream<SyncWriteStream>::value,
       "SyncWriteStream type requirements not met");

    return write(stream, net::buffer(req.payload), ec);
}

template<class SyncWriteStream>
std::size_t write(
   SyncWriteStream& stream,
   pipeline& req)
{
    static_assert(boost::beast::is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream type requirements not met");

    boost::system::error_code ec;
    auto const bytes_transferred = write(stream, req, ec);

    if (ec)
        BOOST_THROW_EXCEPTION(boost::system::system_error{ec});

    return bytes_transferred;
}

/** Asynchronously writes one or more command pipelines on the stream.
 */
template<class AsyncWriteStream>
net::awaitable<void>
async_write_some(
   AsyncWriteStream& stream,
   std::queue<pipeline>& pipelines)
{
   do {
      co_await async_write(stream, net::buffer(pipelines.front().payload), net::use_awaitable);
      pipelines.front().sent = true;
      if (std::empty(pipelines.front().commands)) {
	 // We only pop when all commands in the pipeline has push
	 // responses like subscribe, otherwise, pop is done when the
	 // response arrives.
	 pipelines.pop();
      }
   } while (!std::empty(pipelines) && std::empty(pipelines.front().commands));
}

} // aedis
