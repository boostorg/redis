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
using aedis::resp3::serializer;
using aedis::resp3::async_read;
using aedis::resp3::node;
using aedis::resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;

/** An example on how to serialize containers in a request and read
    them back.
 */

std::string make_request()
{
   std::set<std::string> set
      {"one", "two", "three", "four"};

   std::map<std::string, int> map
      { {"key1", 1}
      , {"key2", 2}
      , {"key3", 3}
      };

   serializer<command> sr;
   sr.push(command::hello, 3);
   sr.push(command::flushall);

   // Set the containers in some of the redis built-in data structures.
   sr.push_range(command::sadd, "key2", std::cbegin(set), std::cend(set));
   sr.push_range(command::hset, "key3", std::cbegin(map), std::cend(map));

   // Retrieves the containers back from redis.
   sr.push(command::smembers, "key2");
   sr.push(command::smembers, "key2");
   sr.push(command::hgetall, "key3");

   return sr.request();
}

net::awaitable<void> containers()
{
   try {
      auto socket = co_await connect();

      // Sends the request to redis.
      auto const req = make_request();
      co_await async_write(socket, buffer(req));

      // The expected responses
      int sadd, hset;
      std::set<std::string> smembers1;
      std::unordered_set<std::string> smembers2;
      std::map<std::string, int> hgetall;

      // Reads the responses.
      std::string buffer;
      co_await async_read(socket, buffer); // hello
      co_await async_read(socket, buffer); // flushall
      co_await async_read(socket, buffer, adapt(sadd)); // sadd
      co_await async_read(socket, buffer, adapt(hset)); // hset
      co_await async_read(socket, buffer, adapt(smembers1)); // smembers
      co_await async_read(socket, buffer, adapt(smembers2)); // smembers
      co_await async_read(socket, buffer, adapt(hgetall)); // hgetall

      // Prints the responses.
      std::cout
         << "sadd: " << sadd << "\n"
         << "hset: " << hset << "\n"
      ;

      std::cout << "smembers1: ";
      for (auto const& e: smembers1) std::cout << e << " ";
      std::cout << std::endl;

      std::cout << "smembers2: ";
      for (auto const& e: smembers2) std::cout << e << " ";
      std::cout << std::endl;

      std::cout << "hgetall: ";
      for (auto const& e: hgetall) std::cout << e.first << " ==> " << e.second << "; ";
      std::cout << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, containers(), net::detached);
   ioc.run();
}

/// \example containers.cpp
