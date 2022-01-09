/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>

#include "utils.ipp"

namespace resp3 = aedis::resp3;
using aedis::command;
using resp3::serializer;
using resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;
using net::dynamic_buffer;

/* Shows how to serialize and read redis hashes in C++ containers.
 */

net::awaitable<void> containers()
{
   try {
      auto socket = co_await connect();

      std::map<std::string, std::string> map
	 { {"key1", "value1"}
	 , {"key2", "value2"}
	 , {"key3", "value3"}
	 };

      // Creates and sends the request.
      serializer<command> sr;
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push_range(command::hset, "key", std::cbegin(map), std::cend(map));
      sr.push(command::hgetall, "key");
      sr.push(command::hgetall, "key");
      sr.push(command::hgetall, "key");
      sr.push(command::quit);
      co_await async_write(socket, buffer(sr.request()));

      // The expected responses
      int hset;
      std::vector<std::string> hgetall1;
      std::map<std::string, std::string> hgetall2;
      std::unordered_map<std::string, std::string> hgetall3;

      // Reads the responses.
      std::string buffer;
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // hello
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // flushall
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(hset));
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(hgetall1));
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(hgetall2));
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(hgetall3));
      co_await resp3::async_read(socket, dynamic_buffer(buffer));

      // Prints the responses.
      std::cout << "hset: " << hset;
      std::cout << "\nhgetall (as vector): ";
      for (auto const& e: hgetall1) std::cout << e << ", ";
      std::cout << "\nhgetall (as map): ";
      for (auto const& e: hgetall2) std::cout << e.first << " ==> " << e.second << "; ";
      std::cout << "\nhgetall (as unordered_map): ";
      for (auto const& e: hgetall3) std::cout << e.first << " ==> " << e.second << "; ";
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
