/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#define BOOST_JSON_NO_LIB
#define BOOST_CONTAINER_NO_LIB
#include <boost/json.hpp>
#include <boost/redis.hpp>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <set>
#include <iterator>
#include <string>
#include "common/common.hpp"

// Include this in no more than one .cpp file.
#include <boost/json/src.hpp>

namespace net = boost::asio;
namespace redis = boost::redis;
using namespace net::experimental::awaitable_operators;
using namespace boost::json;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore_t;

struct user {
   std::string name;
   std::string age;
   std::string country;

   friend auto operator<(user const& a, user const& b)
   {
      return std::tie(a.name, a.age, a.country) < std::tie(b.name, b.age, b.country);
   }

   friend auto operator<<(std::ostream& os, user const& u) -> std::ostream&
   {
      os << "Name: " << u.name << "\n"
         << "Age: " << u.age << "\n"
         << "Country: " << u.country;

      return os;
   }
};

// Boost.Json serialization.
void tag_invoke(value_from_tag, value& jv, user const& u)
{
   jv =
   { {"name", u.name}
   , {"age", u.age}
   , {"country", u.country}
   };
}

template<class T>
void extract(object const& obj, T& t, std::string_view key)
{
   t = value_to<T>(obj.at(key));
}

auto tag_invoke(value_to_tag<user>, value const& jv)
{
   user u;
   object const& obj = jv.as_object();
   extract(obj, u.name, "name");
   extract(obj, u.age, "age");
   extract(obj, u.country, "country");
   return u;
}

// Serialization
void boost_redis_to_bulk(std::string& to, user const& u)
{
   redis::resp3::boost_redis_to_bulk(to, serialize(value_from(u)));
}

void boost_redis_from_bulk(user& u, std::string_view sv, boost::system::error_code&)
{
   value jv = parse(sv);
   u = value_to<user>(jv);
}

net::awaitable<void> co_main(std::string host, std::string port)
{
   std::set<user> users
      {{"Joao", "58", "Brazil"} , {"Serge", "60", "France"}};

   request req;
   req.push("HELLO", 3);
   req.push_range("SADD", "sadd-key", users); // Sends
   req.push("SMEMBERS", "sadd-key"); // Retrieves
   req.push("QUIT");

   response<ignore_t, int, std::set<user>, std::string> resp;

   auto conn = std::make_shared<connection>(co_await net::this_coro::executor);

   co_await connect(conn, host, port);
   co_await (conn->async_run() || conn->async_exec(req, resp));

   for (auto const& e: std::get<2>(resp).value())
      std::cout << e << "\n";
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
