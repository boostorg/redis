/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <queue>
#include <chrono>

#include <boost/asio.hpp>
#include <boost/beast/core/stream_traits.hpp>

#include <aedis/request.hpp>

namespace aedis { namespace resp {

template<
   class SyncWriteStream,
   class Event>
std::size_t
write(
   SyncWriteStream& stream,
   request<Event>& req,
   boost::system::error_code& ec)
{
    static_assert(boost::beast::is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream type requirements not met");

    return write(stream, net::buffer(req.payload), ec);
}

template<
   class SyncWriteStream,
   class Event>
std::size_t
write(
   SyncWriteStream& stream,
   request<Event>& req)
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
   class Event,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncWriteStream::executor_type>>
auto
async_write(
   AsyncWriteStream& stream,
   request<Event>& req,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncWriteStream::executor_type>{})
{
   static_assert(boost::beast::is_async_write_stream<
      AsyncWriteStream>::value,
      "AsyncWriteStream type requirements not met");

   return net::async_write(stream, net::buffer(req.payload), token);
}

template <
   class AsyncWriteStream,
   class Event>
net::awaitable<void>
async_writer(
   AsyncWriteStream& socket,
   std::queue<request<Event>>& reqs,
   net::steady_timer& st)
{
   auto ex = co_await net::this_coro::executor;
   boost::system::error_code ec;
   for (;;) {
      if (!std::empty(reqs)) {
	 ec = {};
	 co_await async_write(
	    socket,
	    reqs.front(),
	    net::redirect_error(net::use_awaitable, ec));
	 if (ec) {
	    std::cerr << "Error: async_writer." << std::endl;
	    co_return;
	 }
      }

      st.expires_after(std::chrono::years{10});
      ec = {};
      co_await st.async_wait(net::redirect_error(net::use_awaitable, ec));
      if (ec == net::error::operation_aborted)
	 continue;

      std::cerr << "Error: async_writer." << std::endl;
      co_return;
   }
}

// Returns true is a write has been triggered.
template <class Event, class Filler>
bool queue_writer(
   std::queue<resp::request<Event>>& reqs,
   Filler filler,
   net::steady_timer& st)
{
   auto const empty = std::empty(reqs);
   if (empty || std::size(reqs) == 1)
      reqs.push({});

   filler(reqs.back());

   if (empty)
      st.cancel();

   return empty;
}

} // resp
} // aedis

