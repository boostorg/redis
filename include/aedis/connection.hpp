/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <memory>
#include <string>

#include <aedis/detail/response_buffers.hpp>

#include "config.hpp"
#include "type.hpp"
#include "request.hpp"
#include "read.hpp"

namespace aedis {

class connection : public std::enable_shared_from_this<connection> {
public:
   struct config {
      std::string host;
      std::string port;
   };

private:
   net::steady_timer timer_;
   net::ip::tcp::socket socket_;
   std::string buffer_;
   resp::response_buffers resps_;
   request_queue reqs_;
   bool reconnect_ = false;
   config conf_;

   void reset();
   net::awaitable<void> worker_coro(receiver_base& recv);

public:
   connection(
      net::io_context& ioc,
      config const& conf = config {"127.0.0.1", "6379"});

   void start(receiver_base& recv);

   template <class Filler>
   void send(Filler filler)
      { queue_writer(reqs_, filler, timer_); }

   void enable_reconnect() noexcept;
};

} // aedis
