/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/client.hpp>

#include <boost/asio/experimental/awaitable_operators.hpp>

namespace aedis {
namespace resp3 {
namespace experimental {

client::client(net::any_io_executor ex)
: socket_{ex}
, timer_{ex}
{ }

net::awaitable<void> client::reader()
{
   // Writes and reads continuosly from the socket.
   for (std::string buffer;;) {
      while (!std::empty(req_info_)) {
	 co_await net::async_write(socket_, net::buffer(requests_.data(), req_info_.front().size));

	 requests_.erase(0, req_info_.front().size);

	 if (req_info_.front().cmds != 0)
	    break; // We must await the responses.

	 req_info_.pop();
      }

      do { // Keeps reading while there are no messages queued waiting to be sent.
	 do { // Consumes the responses to all commands in the request.
	    auto const t = co_await async_read_type(socket_, net::dynamic_buffer(buffer));
	    if (t == type::push) {
	       auto adapter = [this](type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec)
		  {adapter_(command::unknown, t, aggregate_size, depth, data, size, ec);};

	       boost::system::error_code ec;
	       co_await resp3::async_read(socket_, net::dynamic_buffer(buffer), adapter, net::redirect_error(net::use_awaitable, ec));
	       on_msg_(ec, command::unknown);
	    } else {
	       auto adapter = [this](type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec)
		  {adapter_(commands_.front(), t, aggregate_size, depth, data, size, ec);};

	       boost::system::error_code ec;
	       co_await resp3::async_read(socket_, net::dynamic_buffer(buffer), adapter, net::redirect_error(net::use_awaitable, ec));
	       on_msg_(ec, commands_.front());
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
   while (socket_.is_open()) {
      boost::system::error_code ec;
      co_await timer_.async_wait(net::redirect_error(net::use_awaitable, ec));
      while (!std::empty(req_info_)) {
         co_await net::async_write(socket_, net::buffer(requests_.data(), req_info_.front().size));
         requests_.erase(0, req_info_.front().size);
         
         if (req_info_.front().cmds != 0)
            break;
         
         req_info_.pop();
      }
   }
}

net::awaitable<void> client::say_hello()
{
   std::string request;
   auto sr = make_serializer<command>(request);
   sr.push(command::hello, 3);
   co_await net::async_write(socket_, net::buffer(request));

   std::string buffer;
   auto adapter = [this](type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec)
      {adapter_(command::hello, t, aggregate_size, depth, data, size, ec);};
   co_await resp3::async_read(socket_, net::dynamic_buffer(buffer), adapter);
}

net::awaitable<void>
client::connection_manager()
{
   using namespace aedis::net::experimental::awaitable_operators;
   using tcp_resolver = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::ip::tcp::resolver>;

   for (;;) {
      tcp_resolver resolver{socket_.get_executor()};
      auto const res = co_await resolver.async_resolve("127.0.0.1", "6379");
      co_await net::async_connect(socket_, res);

      co_await say_hello();

      timer_.expires_at(std::chrono::steady_clock::time_point::max());
      co_await (reader() && writer());

      socket_.close();
      timer_.cancel();

      timer_.expires_after(std::chrono::seconds{1});
      boost::system::error_code ec;
      co_await timer_.async_wait(net::redirect_error(net::use_awaitable, ec));
   }
}

bool client::prepare_next()
{
   if (std::empty(req_info_)) {
      req_info_.push({});
      return true;
   }

   if (std::size(req_info_) == 1) {
      req_info_.push({});
      return false;
   }

   return false;
}

void client::prepare()
{
   net::co_spawn(socket_.get_executor(),
      [self = this->shared_from_this()]{ return self->connection_manager(); },
      net::detached);
}

void client::set_adapter(adapter_type adapter)
{
   adapter_ = adapter;
}

void client::set_msg_callback(on_message_type on_msg)
{
   on_msg_ = on_msg;
}

} // experimental
} // resp3
} // aedis

