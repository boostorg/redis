/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

// Must come before any asio header, otherwise build fails on msvc.

#include <boost/redis/experimental/connector.hpp>
#include <boost/redis/logger.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <tuple>
#include <iostream>
#include "../examples/start.hpp"

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace net = boost::asio;
namespace redis = boost::redis;
using steady_timer = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;
using redis::request;
using redis::response;
using redis::ignore;
using redis::address;
using redis::logger;
using redis::experimental::async_connect;
using redis::experimental::connect_config;
using connection = boost::asio::use_awaitable_t<>::as_default_on_t<boost::redis::connection>;
using namespace std::chrono_literals;

// Push consumer
auto receiver(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   boost::system::error_code ec;
   while (!ec)
      co_await conn->async_receive(ignore, net::redirect_error(net::use_awaitable, ec));
}

auto periodic_task(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
  net::steady_timer timer{co_await net::this_coro::executor};
  for (int i = 0; i < 10; ++i) {
    timer.expires_after(std::chrono::seconds(2));
    co_await timer.async_wait(net::use_awaitable);

    // Key is not set so it will cause an error since we are passing
    // an adapter that does not accept null, this will cause an error
    // that result in the connection being closed.
    request req;
    req.push("GET", "mykey");
    auto [ec, u] = co_await conn->async_exec(req, ignore, net::as_tuple(net::use_awaitable));
    if (ec) {
      std::cout << "Error: " << ec << std::endl;
    } else {
      std::cout << "no error: " << std::endl;
    }
  }

  std::cout << "Periodic task done!" << std::endl;
  conn->disable_reconnection();
  conn->cancel(redis::operation::run);
  conn->cancel(redis::operation::receive);
}

auto co_main(address const& addr) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);

   connect_config cfg;
   cfg.addr = addr;

   net::co_spawn(ex, receiver(conn), net::detached);
   net::co_spawn(ex, periodic_task(conn), net::detached);
   redis::experimental::async_connect(*conn, cfg, logger{}, net::consign(net::detached, conn));
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
