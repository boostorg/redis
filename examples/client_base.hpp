/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <queue>

#include <aedis/aedis.hpp>

#include "types.hpp"

namespace aedis {
namespace resp3 {

/* An example redis client.
 */
class client_base : public std::enable_shared_from_this<client_base> {
private:
   tcp_socket socket_;
   net::steady_timer timer_;
   std::queue<request> reqs_;

   // Prepares the back of the request queue for new requests.  Returns true
   // when the front of the queue can be sent to the redis server.
   bool prepare_next()
   {
      if (std::empty(reqs_)) {
         // We are not waiting any response.
         reqs_.push({});
         return true;
      }

      // A non-empty queue means we are waiting for a response. Since the
      // reader will automatically send any outstanding requests when the
      // response arrives, we have to return false hier.

      if (std::size(reqs_) == 1) {
         // The user should not append any new commands to this request as it
         // has been already sent to redis.
         reqs_.push({});
      }

      return false;
   }

   net::awaitable<void> reader()
   {
      // Writes and reads continuosly from the socket.
      for (std::string buffer;;) {
         // Writes the first request in queue and all subsequent ones that have
         // no response e.g. subscribe.
         co_await async_write_some(socket_, reqs_);

         // Keeps reading while there is no messages queued waiting to be sent.
         do {
            // Loops to consume the response to all commands in the request.
            do {
               // Reads the type of the incoming response.
               auto const t = co_await async_read_type(socket_, buffer);

               if (t == type::push) {
                  auto& resp = get_response(t);
                  co_await async_read(socket_, buffer, resp);
                  on_event(t);
               } else {
                  auto& resp = get_response(t, reqs_.front().commands.front());
                  co_await async_read(socket_, buffer, resp);
                  on_event(t, reqs_.front().commands.front());
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

   net::awaitable<void> conn_manager()
   {
      tcp_resolver resolver{socket_.get_executor()};
      auto const res = co_await resolver.async_resolve("127.0.0.1", "6379");
      co_await aedis::net::async_connect(socket_, res);

      reqs_.push({});
      reqs_.back().push(command::hello, 3);

      co_spawn(socket_.get_executor(),
               [self = shared_from_this()]{ return self->writer(); },
               net::detached);

      co_await co_spawn(socket_.get_executor(),
                        [self = shared_from_this()]{ return self->reader(); },
                        net::use_awaitable);

      socket_.close();
      timer_.cancel_one();
   }

public:
   client_base(net::any_io_executor ex)
   : socket_{ex}
   , timer_{ex}
   {
      timer_.expires_at(std::chrono::steady_clock::time_point::max());
   }

   ~client_base()
   {
      socket_.close();
      timer_.cancel();
   }

   net::any_io_executor get_executor()
   {
      return socket_.get_executor();
   }

   void start()
   {
      net::co_spawn(socket_.get_executor(),
          [self = shared_from_this()]{ return self->conn_manager(); },
          net::detached);
   }

   /* Adds commands the requests queue and sends if possible.
    */
   template <class Filler>
   void send(Filler filler)
   {
      // Prepares the back of the queue for a new command.
      auto const can_write = prepare_next();

      filler(reqs_.back());

      if (can_write)
         timer_.cancel_one();
   }

   /* @brief Returns the response object the the used wishes to use
    */ 
   virtual response_base&
   get_response(type t, command cmd = command::unknown) = 0;

   /* Function called when data has been aready.
    */
   virtual void
   on_event(type t, command cmd = command::unknown) = 0;

   template <typename Derived>
   std::shared_ptr<Derived> shared_from_base()
   {
       return std::static_pointer_cast<Derived>(shared_from_this());
   }
};

} // resp3
} // aedis

