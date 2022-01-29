/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <vector>
#include <tuple>
#include <array>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "lib/net_utils.hpp"

namespace resp3 = aedis::resp3;
using aedis::redis::command;
using aedis::redis::make_serializer;
using resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;
using net::dynamic_buffer;

net::awaitable<void> transaction()
{
   try {
      auto socket = co_await connect();

      auto list  = {"one", "two", "three"};

      std::string request;
      auto sr = make_serializer(request);
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push(command::multi); // Starts a transaction
      sr.push(command::ping, "Some message");
      sr.push(command::incr, "incr1-key");
      sr.push_range(command::rpush, "list-key", std::cbegin(list), std::cend(list));
      sr.push(command::lrange, "list-key", 0, -1);
      sr.push(command::incr, "incr2-key");
      sr.push(command::exec); // Ends the transaction.
      sr.push(command::quit);
      co_await async_write(socket, buffer(request));

      // Expected responses.
      std::tuple<std::string, int, int, std::vector<std::string>, int> execs;

      // Reads the response.
      std::string buffer;
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // hello
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // flushall
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // multi
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // ping
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // incr
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // rpush
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // lrange
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // incr
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(execs));
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // quit

      // Prints the response to the transaction.
      std::cout << "ping: " << std::get<0>(execs) << "\n";
      std::cout << "incr1: " << std::get<1>(execs) << "\n";
      std::cout << "rpush: " << std::get<2>(execs) << "\n";
      std::cout << "lrange: ";
      for (auto const& e: std::get<3>(execs)) std::cout << e << " ";
      std::cout << "\n";
      std::cout << "incr2: " << std::get<4>(execs) << "\n";

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, transaction(), net::detached);
   ioc.run();
}
