/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

#include <iostream>
#include <set>
#include <vector>
#include <unordered_map>

#include "utils.ipp"

using aedis::command;
using aedis::resp3::serializer;
using aedis::resp3::async_read;
using aedis::resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;

/* An example on how to read sets in C++ containers.
 */

net::awaitable<void> containers()
{
   try {
      auto socket = co_await connect();

      std::set<std::string> set
	 {"one", "two", "three", "four"};

      serializer<command> sr;
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push_range(command::sadd, "key", std::cbegin(set), std::cend(set));
      sr.push(command::smembers, "key");
      sr.push(command::smembers, "key");
      sr.push(command::smembers, "key");
      sr.push(command::quit);
      co_await async_write(socket, buffer(sr.request()));

      // Objects to hold the responses.
      int sadd;
      std::vector<std::string> smembers1;
      std::set<std::string> smembers2;
      std::unordered_set<std::string> smembers3;

      // Reads the responses.
      std::string buffer;
      co_await async_read(socket, buffer); // hello
      co_await async_read(socket, buffer); // flushall
      co_await async_read(socket, buffer, adapt(sadd));
      co_await async_read(socket, buffer, adapt(smembers1));
      co_await async_read(socket, buffer, adapt(smembers2));
      co_await async_read(socket, buffer, adapt(smembers3));
      co_await async_read(socket, buffer);

      // Prints the responses.
      std::cout << "sadd: " << sadd << "\n" ;
      std::cout << "smembers (as vector): ";
      for (auto const& e: smembers1) std::cout << e << " ";
      std::cout << "\n";
      std::cout << "smembers (as set): ";
      for (auto const& e: smembers2) std::cout << e << " ";
      std::cout << "\n";
      std::cout << "smembers (as unordered_set): ";
      for (auto const& e: smembers3) std::cout << e << " ";
      std::cout << "\n";

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

/// \example sets.cpp
