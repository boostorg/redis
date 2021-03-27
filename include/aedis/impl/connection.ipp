/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/connection.hpp>
#include <aedis/write.hpp>

namespace aedis {

connection::connection(net::io_context& ioc, config const& conf)
: timer_{ioc}
, socket_{ioc}
, conf_{conf}
{
   reqs_.push({});
   reqs_.front().req.hello();
}

void connection::reset()
{
   socket_.close();
   timer_.cancel();
   reqs_.push({});
   reqs_.front().req.hello();
}

net::awaitable<void> connection::worker_coro(receiver_base& recv)
{
   auto ex = co_await net::this_coro::executor;

   net::ip::tcp::resolver resolver{ex};
   auto const res = resolver.resolve(conf_.host, conf_.port);

   net::steady_timer timer {ex};
   std::chrono::seconds wait_interval {1};

   boost::system::error_code ec;
   do {
      co_await async_connect(
	 socket_,
	 res,
	 net::redirect_error(net::use_awaitable, ec));

      if (ec) {
	 reset();
	 timer.expires_after(wait_interval);
	 co_await timer.async_wait(net::use_awaitable);
	 continue;
      }

      async_writer(socket_, reqs_, timer_, net::detached);

      ec = {};
      co_await co_spawn(
	 ex,
	 async_reader(socket_, buffer_, resps_, recv, reqs_, ec),
	 net::use_awaitable);

      if (ec) {
	 reset();
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
