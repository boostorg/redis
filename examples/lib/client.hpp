/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <queue>
#include <functional>

#include <aedis/aedis.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include "user_session.hpp"

namespace aedis {
namespace resp3 {

/*  Example: A general purpose redis client.
 *
 *   This class is meant to be an example. Users are meant to derive
 *   from this class and override its virtual functions.
 *
 *      1. on_message.
 *      2. on_push.
 *
 *   The ReponseId type is required to provide the cmd member.
 */
class client : public std::enable_shared_from_this<client> {
public:
   using adapter_type = std::function<void(command, type, std::size_t, std::size_t, char const*, std::size_t, std::error_code&)>;

private:
   using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;

   struct request_info {
      // Request size in bytes.
      std::size_t size = 0;

      // The number of commands it contains excluding commands that
      // have push types as responses, see has_push_response.
      std::size_t cmds = 0;
   };

   // Requests payload.
   std::string requests_;

   // The commands contained in the requests.
   std::queue<command> commands_;

   // Info about the requests.
   std::queue<request_info> req_info_;

   // The stream.
   tcp_socket socket_;

   // Timer used to inform the write coroutine that it can write the
   // next message in the output queue.
   net::steady_timer timer_;

   // Adapter
   adapter_type adapter_;

   // A coroutine that keeps reading the socket. When a message
   // arrives it calls on_message.
   net::awaitable<void> reader()
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
		  auto adapter = [this](type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec){adapter_(command::unknown, t, aggregate_size, depth, data, size, ec);};
                  co_await resp3::async_read(socket_, net::dynamic_buffer(buffer), adapter);
                  on_push();
               } else {
		  auto adapter = [this](type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec){adapter_(commands_.front(), t, aggregate_size, depth, data, size, ec);};
                  co_await resp3::async_read(socket_, net::dynamic_buffer(buffer), adapter);
                  on_message(commands_.front());
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

   // Write coroutine. It is kept suspended until there are messages
   // to be sent.
   net::awaitable<void> writer()
   {
      while (socket_.is_open()) {
         boost::system::error_code ec;
         co_await timer_.async_wait(net::redirect_error(net::use_awaitable, ec));
	 do {
	    assert(!std::empty(req_info_));
	    co_await net::async_write(socket_, net::buffer(requests_.data(), req_info_.front().size));
	    requests_.erase(0, req_info_.front().size);

	    if (req_info_.front().cmds != 0)
	       break;

	    req_info_.pop();
	 } while (!std::empty(req_info_));
      }
   }

   net::awaitable<void> say_hello()
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

   // The connection manager. It keeps trying the reconnect to the
   // server when the connection is lost.
   net::awaitable<void> connection_manager()
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

   /* Prepares the back of the queue to receive further commands. 
    *
    * If true is returned the request in the front of the queue can be
    * sent to the server. See async_write_some.
    */
   bool prepare_next()
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

public:
   // Constructor
   client(net::any_io_executor ex, adapter_type adapter = [](command, type, std::size_t, std::size_t, char const*, std::size_t, std::error_code&) {})
   : socket_{ex}
   , timer_{ex}
   , adapter_{adapter}
   { }

   virtual ~client() { }

   /*  Starts the client.
    *
    *  Stablishes a connection with the redis server and keeps
    *  waiting for messages to send.
    */
   void start()
   {
      net::co_spawn(socket_.get_executor(),
         [self = this->shared_from_this()]{ return self->connection_manager(); },
         net::detached);
   }

   template <class... Ts>
   void send(command cmd, Ts const&... args)
   {
      auto const can_write = prepare_next();

      auto sr = make_serializer<command>(requests_);
      auto const before = std::size(requests_);
      sr.push(cmd, args...);
      auto const after = std::size(requests_);
      req_info_.front().size += after - before;;

      if (!has_push_response(cmd)) {
         commands_.emplace(cmd);
	 ++req_info_.front().cmds;
      }

      if (can_write)
         timer_.cancel_one();
   }

   /* Called when the response to a specific command is received.
    *
    * Override this function to receive events in your derived class.
    */
   virtual void on_message(command) {};

   /* Called when a server push is received.
    *
    * Override this function to receive push events in the derived class.
    */
   virtual void on_push() {};
};

} // resp3
} // aedis

