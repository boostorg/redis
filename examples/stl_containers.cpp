/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>

#include <aedis/aedis.hpp>

#include "types.hpp"
#include "utils.ipp"

using namespace aedis;

net::awaitable<void> stl_containers()
{
   // Helper function to get a connected socket.
   auto socket = co_await make_connection();

   std::vector<int> vec
      {1, 2, 3, 4, 5, 6};

   std::set<std::string> set
      {"one", "two", "three"};

   std::map<std::string, std::string> map
      { {"key1", "value1"}
      , {"key2", "value2"}
      , {"key3", "value3"}
      };

   resp3::request req;
   req.push(command::hello, 3);
   req.push(command::flushall);
   req.push_range(command::rpush, "vector", std::cbegin(vec), std::cend(vec));
   req.push_range(command::sadd, "set", std::cbegin(set), std::cend(set));
   req.push_range(command::hset, "map", std::cbegin(map), std::cend(map));
   co_await async_write(socket, req);

   std::string buffer;
   while (!std::empty(req.commands)) {
      resp3::response resp;
      co_await async_read(socket, buffer, resp);
      std::cout << req.commands.front() << ":\n" << resp << std::endl;
      req.commands.pop();
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, stl_containers(), net::detached);
   ioc.run();
}
