/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <queue>

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
template <class ResponseId>
class client_base
   : public std::enable_shared_from_this<client_base<ResponseId>> {
protected:
   // The response used for push types.
   std::vector<node> push_resp_;

private:
   using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;

   // Hello response.
   std::vector<node> hello_;

   // We are in the middle of a refactoring and there is some mess.
   std::string requests_;
   std::queue<serializer<std::string, ResponseId>> srs_;
   std::queue<std::size_t> req_sizes_;
   tcp_socket socket_;

   // Timer used to inform the write coroutine that it can write the
   // next message in the output queue.
   net::steady_timer timer_;

   // A coroutine that keeps reading the socket. When a message
   // arrives it calls on_message.
   net::awaitable<void> reader()
   {
      // Writes and reads continuosly from the socket.
      for (std::string buffer;;) {
         // Writes the next request in the socket.
	 while (!std::empty(srs_)) {
	    co_await net::async_write(socket_, net::buffer(requests_.data(), req_sizes_.front()));
	    requests_.erase(0, req_sizes_.front());
	    req_sizes_.pop();

	    if (!std::empty(srs_.front().commands))
	       break; // We must await the responses.

	    // Pops the request if no response is expected.
	    srs_.pop();
	 }

         // Keeps reading while there are no messages queued waiting to be sent.
         do {
            // Loops to consume the response to all commands in the request.
            do {
               auto const t =
		  co_await async_read_type(socket_, net::dynamic_buffer(buffer));

               if (t == type::push) {
                  co_await resp3::async_read(socket_, net::dynamic_buffer(buffer), adapt(push_resp_));
                  on_push();
               } else {
                  auto adapter = adapt(*srs_.front().commands.front().resp);
                  co_await resp3::async_read(socket_, net::dynamic_buffer(buffer), adapter);
                  on_message(srs_.front().commands.front());
                  srs_.front().commands.pop();
               }

            } while (!std::empty(srs_) && !std::empty(srs_.front().commands));

            // We may exit the loop above either because we are done
            // with the response or because we received a server push
            // while the queue was empty.
            if (!std::empty(srs_))
               srs_.pop();

         } while (std::empty(srs_));
      }
   }

   // Write coroutine. It is kept suspended until there are messages
   // that can be sent.
   net::awaitable<void> writer()
   {
      while (socket_.is_open()) {
         boost::system::error_code ec;
         co_await timer_.async_wait(net::redirect_error(net::use_awaitable, ec));
	 do {
	    co_await net::async_write(socket_, net::buffer(requests_.data(), req_sizes_.front()));
	    requests_.erase(0, req_sizes_.front());
	    req_sizes_.pop();

	    if (!std::empty(srs_.front().commands))
	       break; // We must await the responses.

	    // Pops the request if no response is expected.
	    srs_.pop();
	 } while (!std::empty(srs_));
      }
   }

   net::awaitable<void> say_hello()
   {
      std::string request;
      auto sr = make_serializer<command>(request);
      sr.push(command::hello, 3);
      co_await net::async_write(socket_, net::buffer(request));

      std::string buffer;
      hello_.clear();
      co_await resp3::async_read(socket_, net::dynamic_buffer(buffer), adapt(hello_));
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
      if (std::empty(srs_)) {
         srs_.push({requests_});
	 req_sizes_.push(0);
         return true;
      }

      if (std::size(srs_) == 1) {
         srs_.push({requests_});
	 req_sizes_.push(0);
         return false;
      }

      return false;
   }

public:
   client_base(net::any_io_executor ex)
   : socket_{ex}
   , timer_{ex}
   { }

   virtual ~client_base() { }

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

   /* Adds commands to the request queue and sends if possible.
    *
    * The filler callable get a request by reference, for example
    *
    * @code
    * void f(serializer& req)
    * {
    *    req.push(command::ping);
    *    ...
    * }
    * @endcode
    *
    * It will be called with the request that is at the back of the queue of
    * outgoing requests.
    */
   template <class Filler>
   void send(Filler filler)
   {
      // Prepares the back of the queue for a new request.
      auto const can_write = prepare_next();

      auto const before = std::size(requests_);
      filler(srs_.back());
      auto const after = std::size(requests_);
      req_sizes_.front() += after - before;;

      if (can_write)
         timer_.cancel_one();
   }

   /* Called when the response to a specific command is received.
    *
    * Override this function to receive events in your derived class.
    */
   virtual void on_message(ResponseId) {};

   /* Called when server push is received.
    *
    * Override this function to receive push events in the derived class.
    */
   virtual void on_push() {};
};

} // resp3
} // aedis

