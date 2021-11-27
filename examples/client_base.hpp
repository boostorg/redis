/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <queue>

#include <aedis/aedis.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include "types.hpp"

using namespace aedis::net::experimental::awaitable_operators;

namespace aedis {
namespace resp3 {

/* A general purpose redis client that supports reads and writes on
 * the same connection.
 */
class client_base : public std::enable_shared_from_this<client_base> {
protected:
   response resp_;

private:
   tcp_socket socket_;

   // A timer used to inform the write coroutine that there is one
   // message awaiting to be send to redis.
   net::steady_timer timer_;
   std::queue<request> reqs_;

   // A coroutine that keeps reading the socket. When a message
   // arrives it calls on_event.
   net::awaitable<void> reader()
   {
      // Writes and reads continuosly from the socket.
      for (std::string buffer;;) {
         // Writes the request in the socket.
         co_await async_write_some(socket_, reqs_);

         // Keeps reading while there is no messages queued waiting to be sent.
         do {
            // Loops to consume the response to all commands in the request.
            do {
               co_await async_read(socket_, buffer, resp_);

               if (resp_.get_type() == type::push) {
                  on_event(command::unknown);
               } else {
                  on_event(reqs_.front().commands.front());
                  reqs_.front().commands.pop();
               }

            } while (!std::empty(reqs_) && !std::empty(reqs_.front().commands));

            // We may exit the loop above either because we are done
            // with the response or because we received a server push
            // while the queue was empty.
            if (!std::empty(reqs_))
               reqs_.pop();

         } while (std::empty(reqs_));
      }
   }

   // Write coroutine. It is kept suspended until there are messages
   // that can be sent.
   net::awaitable<void> writer()
   {
      for (;;) {
         boost::system::error_code ec;
         co_await timer_.async_wait(net::redirect_error(net::use_awaitable, ec));

         if (!socket_.is_open())
            break;

         co_await async_write_some(socket_, reqs_);
      }
   }

   // The connection manager. It keeps trying the reconnect to the
   // server when the connection is lost.
   net::awaitable<void> conn_manager()
   {
      for (;;) {
         tcp_resolver resolver{socket_.get_executor()};
         auto const res = co_await resolver.async_resolve("127.0.0.1", "6379");
         co_await aedis::net::async_connect(socket_, res);

         reqs_.push({});
         reqs_.back().push(command::hello, 3);

         timer_.expires_at(std::chrono::steady_clock::time_point::max());
         co_await (reader() && writer());

         socket_.close();
         timer_.cancel();

         timer_.expires_after(std::chrono::seconds{1});
         boost::system::error_code ec;
         co_await timer_.async_wait(net::redirect_error(net::use_awaitable, ec));
      }
   }

public:
   client_base(net::any_io_executor ex)
   : socket_{ex}
   , timer_{ex}
   {
   }

   ~client_base()
   {
      socket_.close();
      timer_.cancel();
   }

   // Starts the client coroutines.
   void start()
   {
      net::co_spawn(socket_.get_executor(),
          [self = shared_from_this()]{ return self->conn_manager(); },
          net::detached);
   }

   // Adds commands the requests queue and sends if possible.
   template <class Filler>
   void send(Filler filler)
   {
      // Prepares the back of the queue for a new command.
      auto const can_write = prepare_next(reqs_);

      filler(reqs_.back());

      if (can_write)
         timer_.cancel_one();
   }

   // Function called when data has been received.
   virtual void on_event(command cmd) = 0;
};

} // resp3
} // aedis

