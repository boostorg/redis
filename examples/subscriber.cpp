/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <vector>
#include <iostream>
#include <tuple>

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <aedis.hpp>
#include "print.hpp"
#include "reconnect.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using resolver = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::resolver>;
using signal_set_type = net::use_awaitable_t<>::as_default_on_t<net::signal_set>;

using aedis::adapt;
using aedis::resp3::request;
using aedis::resp3::node;

/* This example will subscribe and read pushes indefinitely.
 *
 * To test send messages with redis-cli
 *
 *    $ redis-cli -3
 *    127.0.0.1:6379> PUBLISH channel some-message
 *    (integer) 3
 *    127.0.0.1:6379>
 *
 * To test reconnection try, for example, to close all clients currently
 * connected to the Redis instance
 *
 * $ redis-cli
 * > CLIENT kill TYPE pubsub
 */

// Receives pushes.
auto receiver(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   for (std::vector<node<std::string>> resp;;) {
      co_await conn->async_receive(adapt(resp));
      print_push(resp);
      resp.clear();
   }
}

auto subscriber(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3);
   req.push("SUBSCRIBE", "channel");

   co_await conn->async_exec(req);
}

auto async_main() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   signal_set_type sig{ex, SIGINT, SIGTERM};

   co_await ((run(conn) || healthy_checker(conn) || sig.async_wait() || receiver(conn)) &&
         subscriber(conn));
}

auto main() -> int
{
   try {
      net::io_context ioc{1};
      co_spawn(ioc, async_main(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << "Exception: " << e.what() << std::endl;
      return 1;
   }
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)
auto main() -> int {std::cout << "Requires coroutine support." << std::endl; return 0;}
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
