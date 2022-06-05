/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <boost/asio.hpp>
#include <aedis/aedis.hpp>
#include <aedis/src.hpp>
#include "print.hpp"
#include "mystruct.hpp"

namespace net = boost::asio;
namespace generic = aedis::generic;
using boost::optional;
using aedis::resp3::request;
using aedis::redis::command;
using connection = aedis::generic::connection<command>;

// Response used in this example.
using C1 = std::vector<int>;
using C2 = std::set<mystruct>;
using C3 = std::map<std::string, std::string>;

auto handler =[](auto ec, auto...)
   { std::cout << ec.message() << std::endl; };

int main()
{
   net::io_context ioc;
   connection db{ioc};

   // Request that sends the containers.
   C1 vec {1, 2, 3, 4, 5, 6};
   C2 set {{1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}};
   C3 map {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}};

   request<command> req1;
   req1.push_range(command::rpush, "rpush-key", vec);
   req1.push_range(command::sadd, "sadd-key", set);
   req1.push_range(command::hset, "hset-key", map);

   // Request that retrieves the containers.
   request<command> req2;
   req2.push(command::multi);
   req2.push(command::lrange, "rpush-key", 0, -1);
   req2.push(command::smembers, "sadd-key");
   req2.push(command::hgetall, "hset-key");
   req2.push(command::exec);
   req2.push(command::quit);

   std::tuple<
      std::string, // multi
      std::string, // lrange
      std::string, // smembers
      std::string, // hgetall
      std::tuple<optional<C1>, optional<C2>, optional<C3>>, // exec
      std::string // quit
   > resp;

   db.async_exec(req1, generic::adapt(), handler);
   db.async_exec(req2, generic::adapt(resp), handler);
   db.async_run("127.0.0.1", "6379", handler);
   ioc.run();

   auto const& r = std::get<4>(resp);
   print(std::get<0>(r).value());
   print(std::get<1>(r).value());
   print(std::get<2>(r).value());
}
