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

namespace net = boost::asio;
namespace adapter = aedis::adapter;
using aedis::redis::command;
using aedis::generic::request;
using connection = aedis::generic::connection<command>;
using node_type = aedis::resp3::node<boost::string_view>;
using error_code = boost::system::error_code;

// Response types we use in this example.
using T0 = std::vector<aedis::resp3::node<std::string>>;
using T1 = std::set<std::string>;
using T2 = std::map<std::string, std::string>;

auto handler =[](auto ec, auto...)
   { std::cout << ec.message() << std::endl; };

int main()
{
   T0 resp0;
   T1 resp1;
   T2 resp2;

   auto adapter =
      [ a0 = adapter::adapt(resp0)
      , a1 = adapter::adapt(resp1)
      , a2 = adapter::adapt(resp2)
      ](std::size_t, command cmd, node_type const& nd, error_code& ec) mutable
   {
      switch (cmd) {
         case command::lrange:   a0(nd, ec); break;
         case command::smembers: a1(nd, ec); break;
         case command::hgetall:  a2(nd, ec); break;
         default:;
      }
   };

   net::io_context ioc;
   connection db{ioc};

   std::vector<int> vec
      {1, 2, 3, 4, 5, 6};

   std::set<std::string> set
      {"one", "two", "three", "four"};

   std::map<std::string, std::string> map
      { {"key1", "value1"}
      , {"key2", "value2"}
      , {"key3", "value3"}
      };

   request<command> req;
   req.push_range(command::rpush, "rpush-key", vec);
   req.push_range(command::sadd, "sadd-key", set);
   req.push_range(command::hset, "hset-key", map);
   req.push(command::lrange, "rpush-key", 0, -1);
   req.push(command::smembers, "sadd-key");
   req.push(command::hgetall, "hset-key");
   req.push(command::quit);

   db.async_exec(req, adapter, handler);
   db.async_run(handler);

   ioc.run();

   print_and_clear_aggregate(resp0);
   print_and_clear(resp1);
   print_and_clear(resp2);
}
