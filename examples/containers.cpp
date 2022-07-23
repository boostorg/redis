/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <map>
#include <vector>
#include <iostream>
#include <aedis.hpp>
#include <aedis/src.hpp>
#include "print.hpp"

namespace net = boost::asio;
using boost::optional;
using aedis::adapt;
using aedis::resp3::request;
using connection = aedis::connection<>;

int main()
{
   std::vector<int> vec
      {1, 2, 3, 4, 5, 6};

   std::map<std::string, int> map
      {{"key1", 10}, {"key2", 20}, {"key3", 30}};

   request req;
   req.push("HELLO", 3);
   req.push_range("RPUSH", "rpush-key", vec);
   req.push_range("HSET", "hset-key", map);
   req.push("MULTI");
   req.push("LRANGE", "rpush-key", 0, -1);
   req.push("HGETALL", "hset-key");
   req.push("EXEC");
   req.push("QUIT");

   std::tuple<
      aedis::ignore, // hello
      aedis::ignore, // rpush
      aedis::ignore, // hset
      aedis::ignore, // multi
      aedis::ignore, // lrange
      aedis::ignore, // hgetall
      std::tuple<optional<std::vector<int>>, optional<std::map<std::string, int>>>, // exec
      aedis::ignore  // quit
   > resp;

   net::io_context ioc;
   connection db{ioc};
   db.async_exec("127.0.0.1", "6379", req, aedis::adapt(resp),
      [](auto ec, auto) { std::cout << ec.message() << std::endl; });
   ioc.run();

   print(std::get<0>(std::get<6>(resp)).value());
   print(std::get<1>(std::get<6>(resp)).value());
}
