/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#define BOOST_JSON_NO_LIB
#define BOOST_CONTAINER_NO_LIB
#include <boost/redis.hpp>
#include <boost/describe.hpp>
#include <boost/redis/json.hpp>
#include <set>
#include <string>
#include <iostream>
#include "common/common.hpp"

// Include this in no more than one .cpp file.
#include <boost/json/src.hpp>

namespace net = boost::asio;
namespace redis = boost::redis;
using namespace boost::describe;
using boost::redis::request;
using boost::redis::response;
using boost::redis::operation;
using boost::redis::ignore_t;

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

net::awaitable<void> co_main(std::string host, std::string port)
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   net::co_spawn(ex, run(conn, host, port), net::detached);

   // A set of users that will be automatically serialized to json.
   std::set<user> users
      {{"Joao", "58", "Brazil"} , {"Serge", "60", "France"}};

   // To simplify we send the set and retrieve it in the same
   // resquest.
   request req;
   req.push("HELLO", 3);

   // Stores a std::set in a Redis set data structure.
   req.push_range("SADD", "sadd-key", users);

   // Sends a ping and retrieves it as a string to show what json
   // serialization looks like.
   req.push("PING", *users.begin());

   // Sends another ping and retrieves it directly in a user type.
   req.push("PING", *users.begin());

   // Retrieves the set we have just stored.
   req.push("SMEMBERS", "sadd-key");

   response<ignore_t, ignore_t, std::string, user, std::set<user>> resp;

   // Sends the request and receives the response.
   co_await conn->async_exec(req, resp);

   // Prints the first ping
   auto const& pong1 = std::get<2>(resp).value();
   std::cout << pong1 << "\n";

   // Prints the second ping.
   auto const& pong2 = std::get<3>(resp).value();
   std::cout << pong2.name <<  " " << pong2.age <<  " " << pong2.country <<  "\n";

   // Prints the set.
   for (auto const& e: std::get<4>(resp).value())
      std::cout << e.name << " " << e.age << " " << e.country << "\n";

   conn->cancel(operation::run);
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
