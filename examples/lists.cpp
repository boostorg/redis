/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <deque>

#include "utils.ipp"

using aedis::command;
using aedis::resp3::serializer;
using aedis::resp3::async_read;
using aedis::resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;

net::awaitable<void> ping()
{
   try {
      auto socket = co_await connect();

      // The request.
      std::vector<int> vec {1, 2, 3, 4, 5, 6};
      serializer<command> sr;
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push_range(command::rpush, "key2", std::cbegin(vec), std::cend(vec));
      sr.push(command::lrange, "key2", 0, -1);
      sr.push(command::lrange, "key2", 0, -1);
      sr.push(command::lrange, "key2", 0, -1);
      sr.push(command::lrange, "key2", 0, -1);
      sr.push(command::quit);

      co_await async_write(socket, buffer(sr.request()));

      // Responses and strings and ints in different containers.
      std::vector<std::string> svec;
      std::list<std::string> slist;
      std::deque<std::string> sdeq;
      std::vector<int> ivec;

      std::string rbuffer;
      co_await async_read(socket, rbuffer); // hello
      co_await async_read(socket, rbuffer); // flushall
      co_await async_read(socket, rbuffer); // rpush
      co_await async_read(socket, rbuffer, adapt(svec));
      co_await async_read(socket, rbuffer, adapt(slist));
      co_await async_read(socket, rbuffer, adapt(sdeq));
      co_await async_read(socket, rbuffer, adapt(ivec));
      co_await async_read(socket, rbuffer); // quit.

      for (auto e: svec) std::cout << e << " ";
      std::cout << std::endl;
      for (auto e: slist) std::cout << e << " ";
      std::cout << std::endl;
      for (auto e: sdeq) std::cout << e << " ";
      std::cout << std::endl;
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

/// \example lists.cpp
