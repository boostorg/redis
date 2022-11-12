/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <map>
#include <vector>
#include <iostream>

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <aedis.hpp>
#include "print.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using aedis::adapt;
using aedis::resp3::request;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using connection = aedis::connection<tcp_socket>;

// To avoid verbosity.
auto redir(boost::system::error_code& ec)
{
   return net::redirect_error(net::use_awaitable, ec);
}

// Sends some containers.
auto send(net::ip::tcp::resolver::results_type const& endpoints) -> net::awaitable<void>
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
   co_await net::async_connect(conn.next_layer(), endpoints);
   co_await (conn.async_run() || conn.async_exec(req));
}

// Retrieves a Redis hash as an std::map.
auto
retrieve_hashes(
   net::ip::tcp::resolver::results_type const& endpoints) -> net::awaitable<std::map<std::string, std::string>>
{
   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3);
   req.push("HGETALL", "hset-key");
   req.push("QUIT");

   std::map<std::string, std::string> ret;
   auto resp = std::tie(std::ignore, ret, std::ignore);

   connection conn{co_await net::this_coro::executor};
   co_await net::async_connect(conn.next_layer(), endpoints);
   co_await (conn.async_run() || conn.async_exec(req, adapt(resp)));
   co_return std::move(ret);
}

// Retrieves as a data structure.
auto transaction(net::ip::tcp::resolver::results_type const& endpoints) -> net::awaitable<void>
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
   co_await net::async_connect(conn.next_layer(), endpoints);
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
      auto const hashes = co_await retrieve_hashes(addrs);
      print(hashes);
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
