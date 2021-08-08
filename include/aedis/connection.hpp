/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <memory>
#include <string>

#include <aedis/detail/write.hpp>
#include <aedis/detail/response_buffers.hpp>

#include "net.hpp"
#include "types.hpp"
#include "request.hpp"

namespace aedis {

/** A class that keeps a connection to the redis server.
*/
class connection : public std::enable_shared_from_this<connection> {
public:
   /** Redis server configuration.
   */
   struct config {
      // Redis host.
      std::string host;

      // Redis port.
      std::string port;
   };

private:
   net::steady_timer timer_;
   net::ip::tcp::socket socket_;
   std::string buffer_;
   detail::response_buffers resps_;
   detail::request_queue reqs_;
   bool reconnect_ = false;
   config conf_;

   void reset();
   net::awaitable<void> worker_coro(receiver_base& recv);

public:
   /// Contructs a connection.
   connection(
      net::any_io_executor const& ioc,
      config const& conf = config {"127.0.0.1", "6379"});

   /// Stablishes the connection with the redis server.
   void start(receiver_base& recv);

   /// Adds commands to the ouput queue. The Filler signature must be
   ///
   /// void f(request& req)
   template <class Filler>
   void send(Filler filler)
      { queue_writer(reqs_, filler, timer_); }

   void enable_reconnect() noexcept;
};

} // aedis
