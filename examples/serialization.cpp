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
#include <boost/json.hpp>
#include <boost/json/src.hpp>
#include <aedis.hpp>
#include "print.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::resp3::request;
using aedis::adapt;
using aedis::endpoint;
using connection = aedis::connection<>;
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

user tag_invoke(value_to_tag<user>, value const& jv)
{
    user u;
    object const& obj = jv.as_object();
    extract(obj, u.name, "name");
    extract(obj, u.age, "age");
    extract(obj, u.country, "country");
    return u;
}

// Serializes
void to_bulk(boost::container::pmr::string& to, user const& u)
{
   aedis::resp3::to_bulk(to, serialize(value_from(u)));
}

// Deserializes
void from_bulk(user& u, boost::string_view sv, boost::system::error_code&)
{
   value jv = parse(sv);
   u = value_to<user>(jv);
}

std::ostream& operator<<(std::ostream& os, user const& u)
{
   os << "Name: " << u.name << "\n"
      << "Age: " << u.age << "\n"
      << "Country: " << u.country;

   return os;
}

bool operator<(user const& a, user const& b)
{
   return std::tie(a.name, a.age, a.country) < std::tie(b.name, b.age, b.country);
}

auto logger = [](auto ec, auto...)
   { std::cout << ec.message() << std::endl; };

int main()
{
   net::io_context ioc;
   connection conn{ioc};

   std::set<user> users
      {{"Joao", "58", "Brazil"} , {"Serge", "60", "France"}};

   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3);
   req.push_range("SADD", "sadd-key", users); // Sends
   req.push("SMEMBERS", "sadd-key"); // Retrieves
   req.push("QUIT");

   std::tuple<aedis::ignore, int, std::set<user>, std::string> resp;

   endpoint ep{"127.0.0.1", "6379"};
   conn.async_exec(req, adapt(resp),logger);
   conn.async_run(ep, {}, logger);
   ioc.run();

   // Print
   print(std::get<2>(resp));
}
