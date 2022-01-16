/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <set>
#include <vector>
#include <unordered_map>

#include <aedis/src.hpp>
#include <aedis/aedis.hpp>
#include "utils.ipp"

namespace resp3 = aedis::resp3;
using aedis::command;
using resp3::make_serializer;
using resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;
using net::dynamic_buffer;

/// Shows how to serialize and read redis sets in C++ containers.

net::awaitable<void> containers()
{
   try {
      auto socket = co_await connect();

      std::set<std::string> set
	 {"one", "two", "three", "four"};

      // Creates and sends the request.
      std::string request;
      auto sr = make_serializer<command>(request);
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push_range(command::sadd, "key", std::cbegin(set), std::cend(set));
      sr.push(command::smembers, "key");
      sr.push(command::smembers, "key");
      sr.push(command::smembers, "key");
      sr.push(command::quit);
      co_await async_write(socket, buffer(request));

      // Expected responses.
      int sadd;
      std::vector<std::string> smembers1;
      std::set<std::string> smembers2;
      std::unordered_set<std::string> smembers3;

      // Reads the responses.
      std::string buffer;
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // hello
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // flushall
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(sadd));
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(smembers1));
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(smembers2));
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(smembers3));
      co_await resp3::async_read(socket, dynamic_buffer(buffer));

      // Prints the responses.
      std::cout << "sadd: " << sadd;
      std::cout << "\nsmembers (as vector): ";
      for (auto const& e: smembers1) std::cout << e << " ";
      std::cout << "\nsmembers (as set): ";
      for (auto const& e: smembers2) std::cout << e << " ";
      std::cout << "\nsmembers (as unordered_set): ";
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
