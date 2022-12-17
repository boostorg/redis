/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <aedis.hpp>
#include <map>
#include <vector>

#include "common/common.hpp"

namespace net = boost::asio;
namespace resp3 = aedis::resp3;
using namespace net::experimental::awaitable_operators;
using aedis::adapt;

void print(std::map<std::string, std::string> const& cont)
{
   for (auto const& e: cont)
      std::cout << e.first << ": " << e.second << "\n";
}

void print(std::vector<int> const& cont)
{
   for (auto const& e: cont) std::cout << e << " ";
   std::cout << "\n";
}

// Stores the content of some STL containers in Redis.
auto store() -> net::awaitable<void>
{
   auto conn = std::make_shared<connection>(co_await net::this_coro::executor);

   // Resolves and connects (from examples/common.hpp to avoid vebosity)
   co_await connect(conn, "127.0.0.1", "6379");

   std::vector<int> vec
      {1, 2, 3, 4, 5, 6};

   std::map<std::string, std::string> map
      {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}};

   resp3::request req;
   req.push("HELLO", 3);
   req.push_range("RPUSH", "rpush-key", vec);
   req.push_range("HSET", "hset-key", map);
   req.push("QUIT");

   co_await (conn->async_run() || conn->async_exec(req));
}

auto hgetall() -> net::awaitable<std::map<std::string, std::string>>
{
   auto conn = std::make_shared<connection>(co_await net::this_coro::executor);

   // From examples/common.hpp to avoid vebosity
   co_await connect(conn, "127.0.0.1", "6379");

   // A request contains multiple commands.
   resp3::request req;
   req.push("HELLO", 3);
   req.push("HGETALL", "hset-key");
   req.push("QUIT");

   // Responses as tuple elements.
   std::tuple<aedis::ignore, std::map<std::string, std::string>, aedis::ignore> resp;

   // Executes the request and reads the response.
   co_await (conn->async_run() || conn->async_exec(req, adapt(resp)));
   co_return std::get<1>(resp);
}

// Retrieves in a transaction.
auto transaction() -> net::awaitable<void>
{
   auto conn = std::make_shared<connection>(co_await net::this_coro::executor);

   // Resolves and connects (from examples/common.hpp to avoid vebosity)
   co_await connect(conn, "127.0.0.1", "6379");

   resp3::request req;
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

   co_await (conn->async_run() || conn->async_exec(req, adapt(resp)));

   print(std::get<0>(std::get<4>(resp)).value());
   print(std::get<1>(std::get<4>(resp)).value());
}

// Called from the main function (see main.cpp)
net::awaitable<void> async_main()
{
   co_await store();
   co_await transaction();
   auto const map = co_await hgetall();
   print(map);
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
