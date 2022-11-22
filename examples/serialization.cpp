/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <set>
#include <iterator>
#include <string>

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/json.hpp>
#include <aedis.hpp>
#include "print.hpp"
#include "reconnect.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>
#include <boost/json/src.hpp>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using aedis::resp3::request;
using aedis::adapt;
using namespace boost::json;

struct user {
   std::string name;
   std::string age;
   std::string country;
};

void tag_invoke(value_from_tag, value& jv, user const& u)
{
   jv =
   { {"name", u.name}
   , {"age", u.age}
   , {"country", u.country}
   };
}

template<class T>
void extract(object const& obj, T& t, boost::string_view key)
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

// Serializes
void to_bulk(std::pmr::string& to, user const& u)
{
   aedis::resp3::to_bulk(to, serialize(value_from(u)));
}

// Deserializes
void from_bulk(user& u, boost::string_view sv, boost::system::error_code&)
{
   value jv = parse(sv);
   u = value_to<user>(jv);
}

auto operator<<(std::ostream& os, user const& u) -> std::ostream&
{
   os << "Name: " << u.name << "\n"
      << "Age: " << u.age << "\n"
      << "Country: " << u.country;

   return os;
}

auto operator<(user const& a, user const& b)
{
   return std::tie(a.name, a.age, a.country) < std::tie(b.name, b.age, b.country);
}

net::awaitable<void> exec(std::shared_ptr<connection> conn)
{
   std::set<user> users
      {{"Joao", "58", "Brazil"} , {"Serge", "60", "France"}};

   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3);
   req.push_range("SADD", "sadd-key", users); // Sends
   req.push("SMEMBERS", "sadd-key"); // Retrieves
   req.push("QUIT");

   std::tuple<aedis::ignore, int, std::set<user>, std::string> resp;

   co_await conn->async_exec(req, adapt(resp));

   // Print
   print(std::get<2>(resp));
}

net::awaitable<void> async_main()
{
   auto conn = std::make_shared<connection>(co_await net::this_coro::executor);

   co_await (run(conn) || exec(conn));
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
