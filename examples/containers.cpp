/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <map>
#include <vector>
#include <iostream>
#include <aedis.hpp>
#include "print.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using aedis::endpoint;
using connection = aedis::connection<>;

int main()
{
   std::vector<int> vec
      {1, 2, 3, 4, 5, 6};

   std::map<std::string, int> map
      {{"key1", 10}, {"key2", 20}, {"key3", 30}};

   // Sends and retrieves containers in the same request for
   // simplification.
   request req;
   req.push_range("RPUSH", "rpush-key", vec); // Sends
   req.push_range("HSET", "hset-key", map); // Sends
   req.push("MULTI");
   req.push("LRANGE", "rpush-key", 0, -1); // Retrieves
   req.push("HGETALL", "hset-key"); // Retrieves
   req.push("EXEC");
   req.push("QUIT");

   std::tuple<
      aedis::ignore, // rpush
      aedis::ignore, // hset
      aedis::ignore, // multi
      aedis::ignore, // lrange
      aedis::ignore, // hgetall
      std::tuple<std::optional<std::vector<int>>, std::optional<std::map<std::string, int>>>, // exec
      aedis::ignore  // quit
   > resp;

   net::io_context ioc;
   connection db{ioc};
   endpoint ep{"127.0.0.1", "6379"};
   db.async_run(ep, req, adapt(resp), [](auto ec, auto) {
      std::cout << ec.message() << std::endl;
   });
   ioc.run();

   print(std::get<0>(std::get<5>(resp)).value());
   print(std::get<1>(std::get<5>(resp)).value());
}
