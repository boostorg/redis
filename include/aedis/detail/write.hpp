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

namespace aedis { namespace detail {

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

// TODO: Implement as a composed operation.
template <class AsyncReadWriteStream>
net::awaitable<void>
async_write_all(
   AsyncReadWriteStream& socket,
   std::queue<pipeline>& reqs,
   boost::system::error_code& ec)
{
   // Commands like unsubscribe have a push response so we do not
   // have to wait for a response before sending a new pipeline.
   while (!std::empty(reqs) && !reqs.front().writing) {
      reqs.front().writing = true;
      auto buffer = net::buffer(reqs.front().payload);
      co_await async_write(
	 socket,
	 buffer,
	 net::redirect_error(net::use_awaitable, ec));

      if (ec) {
	 reqs.front().writing = false;
	 co_return;
      }

      if (!std::empty(reqs.front().cmds))
	 break;

      reqs.pop();
   }
}

} // detail
} // aedis
