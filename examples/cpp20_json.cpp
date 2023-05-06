/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/describe.hpp>
#include <boost/asio/consign.hpp>
#include <string>
#include <iostream>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)

#define BOOST_JSON_NO_LIB
#define BOOST_CONTAINER_NO_LIB
#include "json.hpp"
#include <boost/json/src.hpp>

namespace net = boost::asio;
using namespace boost::describe;
using connection = net::deferred_t::as_default_on_t<boost::redis::connection>;
using boost::redis::request;
using boost::redis::response;
using boost::redis::operation;
using boost::redis::ignore_t;
using boost::redis::config;

// Struct that will be stored in Redis using json serialization. 
struct user {
   std::string name;
   std::string age;
   std::string country;
};

// The type must be described for serialization to work.
BOOST_DESCRIBE_STRUCT(user, (), (name, age, country))

// Boost.Redis customization points (examples/json.hpp)
void boost_redis_to_bulk(std::string& to, user const& u)
   { boost::redis::json::to_bulk(to, u); }

void boost_redis_from_bulk(user& u, std::string_view sv, boost::system::error_code& ec)
   { boost::redis::json::from_bulk(u, sv, ec); }

net::awaitable<void> co_main(config const& cfg)
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   conn->async_run(cfg, {}, net::consign(net::detached, conn));

   // user object that will be stored in Redis in json format.
   user const u{"Joao", "58", "Brazil"};

   // Stores and retrieves in the same request.
   request req;
   req.push("SET", "json-key", u); // Stores in Redis.
   req.push("GET", "json-key"); // Retrieves from Redis.

   response<ignore_t, user> resp;

   co_await conn->async_exec(req, resp);
   conn->cancel();

   // Prints the first ping
   std::cout
      << "Name: " << std::get<1>(resp).value().name << "\n"
      << "Age: " << std::get<1>(resp).value().age << "\n"
      << "Country: " << std::get<1>(resp).value().country << "\n";
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
