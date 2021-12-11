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
using aedis::resp3::async_read;
using aedis::resp3::async_write;
using aedis::resp3::node;
using aedis::resp3::adapt;

namespace net = aedis::net;

/** An example on how to serialize containers in a request and read them back.
 */

request<command>
make_request()
{
   std::vector<int> vec
      {1, 2, 3, 4, 5, 6};

   std::set<std::string> set
      {"one", "two", "three"};

   std::map<std::string, std::string> map
      { {"key1", "value1"}
      , {"key2", "value2"}
      , {"key3", "value3"}
      };

   request<command> req;
   req.push(command::hello, 3);
   req.push(command::flushall);

   // Set the containers in some of the redis built-in data structures.
   req.push_range(command::rpush, "key1", std::cbegin(vec), std::cend(vec));
   req.push_range(command::sadd, "key2", std::cbegin(set), std::cend(set));
   req.push_range(command::hset, "key3", std::cbegin(map), std::cend(map));

   // Retrieves the containers back from redis.
   req.push(command::lrange, "key1", 0, -1);
   req.push(command::smembers, "key2");

   // TODO: Request the other containers.
   return req;
}

net::awaitable<void> stl_containers()
{
   try {
      auto socket = co_await connect();
      auto const req = make_request();

      co_await async_write(socket, req);

      // The responses
      int rpush, sadd, hset;
      std::vector<int> lrange;
      std::set<std::string> smembers;

      std::string buffer;
      co_await async_read(socket, buffer); // hello
      co_await async_read(socket, buffer); // flushall
      co_await async_read(socket, buffer, adapt(rpush)); // rpush
      co_await async_read(socket, buffer, adapt(sadd)); // sadd
      co_await async_read(socket, buffer, adapt(hset)); // hset
      co_await async_read(socket, buffer, adapt(lrange)); // lrange
      co_await async_read(socket, buffer, adapt(smembers)); // smembers

      std::cout
         << "rpush: " << rpush << "\n"
         << "sadd: " << sadd << "\n"
         << "hset: " << hset << "\n"
      ;

      std::cout << "lrange: ";
      for (auto e: lrange) std::cout << e << " ";
      std::cout << std::endl;

      std::cout << "smembers: ";
      for (auto e: smembers) std::cout << e << " ";
      std::cout << std::endl;

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
