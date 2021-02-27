/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <chrono>

#include "config.hpp"
#include "request.hpp"

#include <boost/beast/core/stream_traits.hpp>

namespace aedis {

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
  class AsyncReadStream,
  class Event>
struct writer_op {
   AsyncReadStream& stream;
   net::steady_timer& st;
   request_queue<Event>* reqs;

   template <class Self>
   void operator()(
      Self& self,
      boost::system::error_code ec = {},
      std::size_t bytes_transferred = 0)
   {
      // To stop the operation users are required to close the socket
      // and cancel the timer.
      if (!stream.is_open()) {
         self.complete({});
         return;
      }

      // We don't leave on operation aborted as that is the error we
      // get when users cancel the timer to signal there is message to
      // be written.  This relies on the fact that async_write does
      // not complete with this error.
      if (ec && ec != net::error::operation_aborted) {
	 reqs->front().sent = false;
         self.complete(ec);
         return;
      }

      // When b1 is true we are coming from a successful write.
      auto const b1 = bytes_transferred != 0;

      // When b2 is true there is no message to be written.
      auto const b2 = std::empty(*reqs);

      // When b3 is true the message in the queue has already be sent.
      auto const b3 = reqs->front().sent;

      if (b1 || b2 || b3) {
	 st.expires_after(std::chrono::years{10});
	 st.async_wait(std::move(self));
      } else {
	 reqs->front().sent = true;
	 async_write(
	    stream,
	    net::buffer(reqs->front().req.payload),
	    std::move(self));
      }
   }
};

template <
   class AsyncWriteStream,
   class Event,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncWriteStream::executor_type>
   >
auto async_writer(
   AsyncWriteStream& stream,
   request_queue<Event>& reqs,
   net::steady_timer& writeTrigger,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncWriteStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(writer_op<AsyncWriteStream, Event> {stream, writeTrigger, &reqs},
        token,
        stream,
	writeTrigger);
}

// Returns true if a write has been triggered.
template <class Event, class Filler>
bool queue_writer(
   request_queue<Event>& reqs,
   Filler filler,
   net::steady_timer& st)
{
   auto const empty = std::empty(reqs);
   if (empty || std::size(reqs) == 1)
      reqs.push({});

   filler(reqs.back().req);

   if (empty)
      st.cancel();

   return empty;
}
} // aedis

