/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/redis.hpp>
#include <iostream>
#include <set>
#include <string>
#include "common/common.hpp"
#include "common/serialization.hpp"

// Include this in no more than one .cpp file.
#include <boost/json/src.hpp>

namespace net = boost::asio;
namespace redis = boost::redis;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore_t;
using boost::redis::operation;

struct user {
   std::string name;
   std::string age;
   std::string country;

   friend
   auto operator<(user const& a, user const& b)
      { return std::tie(a.name, a.age, a.country) < std::tie(b.name, b.age, b.country); }
};

BOOST_DESCRIBE_STRUCT(user, (), (name, age, country))

auto run(std::shared_ptr<connection> conn, std::string host, std::string port) -> net::awaitable<void>
{
   co_await connect(conn, host, port);
   co_await conn->async_run();
}

auto hello(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   request req;
   req.push("HELLO", 3);

   co_await conn->async_exec(req);
}

auto sadd(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   std::set<user> users
      {{"Joao", "58", "Brazil"} , {"Serge", "60", "France"}};

   request req;
   req.push_range("SADD", "sadd-key", users); // Sends

   co_await conn->async_exec(req);
}

auto smembers(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   request req;
   req.push("SMEMBERS", "sadd-key");

   response<std::set<user>> resp;

   co_await conn->async_exec(req, resp);

   for (auto const& e: std::get<0>(resp).value())
      std::cout << e << "\n";
}

net::awaitable<void> co_main(std::string host, std::string port)
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   net::co_spawn(ex, run(conn, host, port), net::detached);
   co_await hello(conn);
   co_await sadd(conn);
   co_await smembers(conn);
   conn->cancel(operation::run);
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
