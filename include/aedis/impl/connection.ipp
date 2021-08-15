/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/connection.hpp>
#include <aedis/detail/read.hpp>
#include <aedis/detail/write.hpp>

namespace aedis {

connection::connection(net::any_io_executor const& ioc, config const& conf)
: socket_{ioc}
, resps_{buffers_}
, conf_{conf}
{
}

net::awaitable<void> connection::worker_coro(receiver_base& recv)
{
   auto ex = co_await net::this_coro::executor;

   net::ip::tcp::resolver resolver{ex};
   auto const res = resolver.resolve(conf_.host, conf_.port);

   net::steady_timer timer {ex};
   std::chrono::seconds wait_interval {1};

   do {
      boost::system::error_code ec;
      co_await async_connect(
	 socket_,
	 res,
	 net::redirect_error(net::use_awaitable, ec));

      if (ec) {
	 socket_.close();
	 timer.expires_after(wait_interval);
	 co_await timer.async_wait(net::use_awaitable);
	 continue;
      }

      send([](auto& req) { req.hello(); });

      co_await detail::async_reader(socket_, buffer_, resps_, recv, reqs_, ec);

      if (ec) {
	 socket_.close();
	 timer.expires_after(wait_interval);
	 co_await timer.async_wait(net::use_awaitable);
	 continue;
      }
   } while (reconnect_);
}

void connection::start(receiver_base& recv)
{
   auto self = this->shared_from_this();
   net::co_spawn(
      socket_.get_executor(),
      [self, &recv] () mutable { return self->worker_coro(recv); },
      net::detached);
}

void connection::enable_reconnect() noexcept
{
   reconnect_ = false;
}

} // aedis
