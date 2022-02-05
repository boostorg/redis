/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/redis/experimental/client.hpp>

#include <boost/asio/experimental/awaitable_operators.hpp>

namespace aedis {
namespace resp3 {
namespace experimental {

client::client(net::any_io_executor ex)
: socket_{ex}
, timer_{ex}
{
   timer_.expires_at(std::chrono::steady_clock::time_point::max());
}

net::awaitable<void> client::reader()
{
   // Writes and reads continuosly from the socket.
   for (std::string buffer;;) {
      // Notice this coro can get scheduled while the write operation
      // in the writer is ongoing. so we have to check.
      while (!std::empty(req_info_) && req_info_.front().size != 0) {
         assert(!std::empty(requests_));
         boost::system::error_code ec;
         co_await
            net::async_write(
               socket_,
               net::buffer(requests_.data(), req_info_.front().size),
               net::redirect_error(net::use_awaitable, ec));

	 requests_.erase(0, req_info_.front().size);
         req_info_.front().size = 0;

	 if (req_info_.front().cmds != 0)
	    break; // We must await the responses.

	 req_info_.pop();
      }

      do { // Keeps reading while there are no messages queued waiting to be sent.
	 do { // Consumes the responses to all commands in the request.
            boost::system::error_code ec;
	    auto const t =
               co_await async_read_type(socket_, net::dynamic_buffer(buffer),
                     net::redirect_error(net::use_awaitable, ec));
            if (ec) {
               stop_writer_ = true;
               timer_.cancel();
               co_return;
            }

	    if (t == type::push) {
	       auto adapter = [this](type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec)
		  {extended_adapter_(redis::command::unknown, t, aggregate_size, depth, data, size, ec);};

	       co_await resp3::async_read(socket_, net::dynamic_buffer(buffer), adapter, net::redirect_error(net::use_awaitable, ec));
	       on_msg_(ec, redis::command::unknown);
               if (ec) { // TODO: Return only on non aedis errors.
                  stop_writer_ = true;
                  timer_.cancel();
                  co_return;
               }

	    } else {
	       auto adapter = [this](type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec)
		  {extended_adapter_(commands_.front(), t, aggregate_size, depth, data, size, ec);};

	       boost::system::error_code ec;
	       co_await resp3::async_read(socket_, net::dynamic_buffer(buffer), adapter, net::redirect_error(net::use_awaitable, ec));
	       on_msg_(ec, commands_.front());
               if (ec) { // TODO: Return only on non aedis errors.
                  stop_writer_ = true;
                  timer_.cancel();
                  co_return;
               }

	       commands_.pop();
	       --req_info_.front().cmds;
	    }

	 } while (!std::empty(req_info_) && req_info_.front().cmds != 0);

	 // We may exit the loop above either because we are done
	 // with the response or because we received a server push
	 // while the queue was empty so we have to check before
	 // poping..
	 if (!std::empty(req_info_))
	    req_info_.pop();

      } while (std::empty(req_info_));
   }
}

net::awaitable<void> client::writer()
{
   boost::system::error_code ec;
   while (socket_.is_open()) {
      ec = {};
      co_await timer_.async_wait(net::redirect_error(net::use_awaitable, ec));
      if (stop_writer_)
         co_return;

      // Notice this coro can get scheduled while the write operation
      // in the reader is ongoing. so we have to check.
      while (!std::empty(req_info_) && req_info_.front().size != 0) {
         assert(!std::empty(requests_));
         ec = {};
         co_await net::async_write(
            socket_, net::buffer(requests_.data(), req_info_.front().size),
            net::redirect_error(net::use_awaitable, ec));
         if (ec) {
           // What should we do here exactly? Closing the socket will
           // cause the reader coroutine to return so that the engage
           // coroutine returns to the user.
           socket_.close();
           co_return;
         }

         requests_.erase(0, req_info_.front().size);
         req_info_.front().size = 0;
         
         if (req_info_.front().cmds != 0) 
            break;
         
         req_info_.pop();
      }
   }
}

bool client::prepare_next()
{
   if (std::empty(req_info_)) {
      req_info_.push({});
      return true;
   }

   if (req_info_.front().size == 0) {
      // It has already been written and we are waiting for the
      // responses.
      req_info_.push({});
      return false;
   }

   return false;
}

void client::set_extended_adapter(extented_adapter_type adapter)
{
   extended_adapter_ = adapter;
}

void client::set_msg_callback(on_message_type on_msg)
{
   on_msg_ = on_msg;
}

net::awaitable<void> client::engage(socket_type socket)
{
   using namespace aedis::net::experimental::awaitable_operators;

   socket_ = std::move(socket);

   std::string request;
   auto sr = redis::make_serializer(request);
   sr.push(redis::command::hello, 3);

   boost::system::error_code ec;
   co_await net::async_write(socket_, net::buffer(request), net::redirect_error(net::use_awaitable, ec));
   if (ec)
      co_return;

   std::string buffer;
   auto adapter = [this](type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec)
      {extended_adapter_(redis::command::hello, t, aggregate_size, depth, data, size, ec);};

   co_await
      resp3::async_read(
         socket_, net::dynamic_buffer(buffer), adapter, net::redirect_error(net::use_awaitable, ec));

   on_msg_(ec, redis::command::hello);

   if (ec)
      co_return;

   co_await (reader() && writer());
}

} // experimental
} // resp3
} // aedis

