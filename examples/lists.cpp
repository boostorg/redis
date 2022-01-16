/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <deque>

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

/* Shows how to work with redis lists.
 
   First we store a list of elements in a redis list, then the list is
   read back in different C++ containers to give the user a feeling of
   what is possible.

   To store your own types in the lists see serialization.cpp.
*/

net::awaitable<void> ping()
{
   try {
      auto socket = co_await connect();

      // Creates and sends the request.
      auto vec  = {1, 2, 3, 4, 5, 6};
      std::string request;
      auto sr = make_serializer<command>(request);
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push_range(command::rpush, "key", std::cbegin(vec), std::cend(vec));
      sr.push(command::lrange, "key", 0, -1);
      sr.push(command::lrange, "key", 0, -1);
      sr.push(command::lrange, "key", 0, -1);
      sr.push(command::lrange, "key", 0, -1);
      sr.push(command::quit);
      co_await async_write(socket, buffer(request));

      // Expected responses.
      int rpush;
      std::vector<std::string> svec;
      std::list<std::string> slist;
      std::deque<std::string> sdeq;
      std::vector<int> ivec;

      // Reads the responses.
      std::string rbuffer;
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer)); // hello
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer)); // flushall
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer), adapt(rpush)); // rpush
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer), adapt(svec));
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer), adapt(slist));
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer), adapt(sdeq));
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer), adapt(ivec));
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer)); // quit

      // Prints the responses.
      std::cout << "rpush: " << rpush;
      std::cout << "\nlrange (as vector): ";
      for (auto e: svec) std::cout << e << " ";
      std::cout << "\nlrange (as list): ";
      for (auto e: slist) std::cout << e << " ";
      std::cout << "\nlrange (as deque): ";
      for (auto e: sdeq) std::cout << e << " ";
      std::cout << "\nlrange (as vector<int>): ";
      for (auto e: ivec) std::cout << e << " ";
      std::cout << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, ping(), net::detached);
   ioc.run();
}
