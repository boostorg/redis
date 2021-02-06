/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <memory>

#include "config.hpp"
#include "type.hpp"
#include "request.hpp"

namespace aedis {

template <class Event>
class connection :
   public std::enable_shared_from_this<connection<Event>> {
private:
   net::steady_timer timer_;
   net::ip::tcp::socket socket_;
   std::queue<resp::request<Event>> reqs_;

   void finish()
   {
      socket_.close();
      timer_.cancel();
   }

   template <class Receiver>
   net::awaitable<void>
   worker_coro(
      Receiver& recv,
      typename boost::asio::ip::tcp::resolver::results_type const& results)
   {
      auto ex = co_await net::this_coro::executor;

      boost::system::error_code ec;

      co_await async_connect(
	 socket_,
	 results,
	 net::redirect_error(net::use_awaitable, ec));

      if (ec) {
	 finish();
	 recv.on_error(ec);
	 co_return;
      }

      resp::async_writer(socket_, reqs_, timer_, net::detached);

      ec = {};
      co_await co_spawn(
	 ex,
	 resp::async_reader(socket_, recv, reqs_),
	 net::redirect_error(net::use_awaitable, ec));

      if (ec) {
	 finish();
	 recv.on_error(ec);
	 co_return;
      }
   }

public:
   using event_type = Event;

   connection(net::io_context& ioc)
   : timer_{ioc}
   , socket_{ioc}
   {
      reqs_.push({});
      reqs_.front().hello();
   }

   template <class Receiver>
   void
   start(
      Receiver& recv,
      typename boost::asio::ip::tcp::resolver::results_type const& results)
   {
      auto self = this->shared_from_this();
      net::co_spawn(
	 socket_.get_executor(),
         [self, &recv, &results] () mutable { return self->worker_coro(recv, results); },
         net::detached);
   }

   template <class Filler>
   void send(Filler filler)
      { queue_writer(reqs_, filler, timer_); }
};

} // aedis

