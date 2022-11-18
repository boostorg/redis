/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <aedis.hpp>
#include "print.hpp"

#include <map>
#include <vector>
#include <iostream>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using endpoints = net::ip::tcp::resolver::results_type;

using aedis::adapt;
using aedis::resp3::request;
using connection = net::use_awaitable_t<>::as_default_on_t<aedis::connection<>>;

// Sends some containers.
auto send(endpoints const& addrs) -> net::awaitable<void>
{
   std::vector<int> vec
      {1, 2, 3, 4, 5, 6};

   std::map<std::string, std::string> map
      {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}};

   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3);
   req.push_range("RPUSH", "rpush-key", vec);
   req.push_range("HSET", "hset-key", map);
   req.push("QUIT");

   connection conn{co_await net::this_coro::executor};
   co_await net::async_connect(conn.next_layer(), addrs);
   co_await (conn.async_run() || conn.async_exec(req));
}

// Retrieves a Redis hash as an std::map.
auto hgetall(endpoints const& addrs) -> net::awaitable<void>
{
   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3);
   req.push("HGETALL", "hset-key");
   req.push("QUIT");

   std::tuple<aedis::ignore, std::map<std::string, std::string>, aedis::ignore> resp;

   connection conn{co_await net::this_coro::executor};
   co_await net::async_connect(conn.next_layer(), addrs);
   co_await (conn.async_run() || conn.async_exec(req, adapt(resp)));

   print(std::get<1>(resp));
}

// Retrieves as a data structure.
auto transaction(net::ip::tcp::resolver::results_type const& addrs) -> net::awaitable<void>
{
   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3);
   req.push("MULTI");
   req.push("LRANGE", "rpush-key", 0, -1); // Retrieves
   req.push("HGETALL", "hset-key"); // Retrieves
   req.push("EXEC");
   req.push("QUIT");

   std::tuple<
      aedis::ignore, // hello
      aedis::ignore, // multi
      aedis::ignore, // lrange
      aedis::ignore, // hgetall
      std::tuple<std::optional<std::vector<int>>, std::optional<std::map<std::string, std::string>>>, // exec
      aedis::ignore  // quit
   > resp;

   connection conn{co_await net::this_coro::executor};
   co_await net::async_connect(conn.next_layer(), addrs);
   co_await (conn.async_run() || conn.async_exec(req, adapt(resp)));

   print(std::get<0>(std::get<4>(resp)).value());
   print(std::get<1>(std::get<4>(resp)).value());
}

net::awaitable<void> async_main()
{
   try {
      net::ip::tcp::resolver resv{co_await net::this_coro::executor};
      auto addrs = co_await resv.async_resolve("127.0.0.1", "6379", net::use_awaitable);

      co_await send(addrs);
      co_await transaction(addrs);
      co_await hgetall(addrs);
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

auto main() -> int
{
   try {
      net::io_context ioc;
      net::co_spawn(ioc, async_main(), net::detached);
      ioc.run();
   } catch (...) {
      std::cerr << "Error." << std::endl;
   }
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)
auto main() -> int {std::cout << "Requires coroutine support." << std::endl; return 0;}
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
