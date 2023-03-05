/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

// Must come before any asio header, otherwise build fails on msvc.
#include <tuple>

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/redis.hpp>
#include <boost/redis/experimental/run.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include "../examples/common/common.hpp"

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using steady_timer = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;
using boost::redis::experimental::async_check_health;

// Push consumer
auto receiver(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
  for (;;)
    co_await conn->async_receive();
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
}

auto co_main(std::string host, std::string port) -> net::awaitable<void>
{
  auto ex = co_await net::this_coro::executor;
  auto conn = std::make_shared<connection>(ex);
  steady_timer timer{ex};

  request req;
  req.push("HELLO", 3);
  req.push("SUBSCRIBE", "channel");

  // The loop will reconnect on connection lost. To exit type Ctrl-C twice.
  for (int i = 0; i < 10; ++i) {
    co_await connect(conn, host, port);
    co_await ((conn->async_run() || receiver(conn) || async_check_health(*conn) || periodic_task(conn)) &&
              conn->async_exec(req));

    conn->reset_stream();
    timer.expires_after(std::chrono::seconds{1});
    co_await timer.async_wait();
  }
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
