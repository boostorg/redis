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

using aedis::command;
using aedis::resp3::request;
using aedis::resp3::response;
using aedis::resp3::async_read;
using aedis::resp3::async_write;

namespace net = aedis::net;

net::awaitable<void> stl_containers()
{
   try {
      auto socket = co_await make_connection("127.0.0.1", "6379");

      request<command> req;

      // hello with version 3 is always required.
      req.push(command::hello, 3);

      // Sends a flushall to avoid hitting an existing that happen to contain a
      // different data type.
      req.push(command::flushall);

      // rpush with a vector.
      std::vector<int> vec
         {1, 2, 3, 4, 5, 6};

      req.push_range(command::rpush, "key1", std::cbegin(vec), std::cend(vec));

      // sadd with a set.
      std::set<std::string> set
         {"one", "two", "three"};

      req.push_range(command::sadd, "key2", std::cbegin(set), std::cend(set));
      std::cout << "cc" << std::endl;

      // hset with a map.
      std::map<std::string, std::string> map
         { {"key1", "value1"}
         , {"key2", "value2"}
         , {"key3", "value3"}
         };

      req.push_range(command::hset, "key3", std::cbegin(map), std::cend(map));

      // Communication with the redis server starts here.
      co_await async_write(socket, req);

      std::string buffer;
      while (!std::empty(req.commands)) {
         response resp;
         co_await async_read(socket, buffer, resp);

         std::cout
            << req.commands.front() << ":\n"
            << resp << std::endl;

         req.commands.pop();
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, stl_containers(), net::detached);
   ioc.run();
}


/// \example containers.cpp
