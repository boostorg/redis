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
using resp3::node;

namespace net = aedis::net;
using net::async_write;
using net::buffer;
using net::dynamic_buffer;

// Reads the response to a transaction in a general format that is
// suitable for all kinds of responses, but which users may have to
// convert into their own desired format.
net::awaitable<void> nested_response()
{
   try {
      auto socket = co_await connect();

      auto list  = {"one", "two", "three"};

      std::string request;
      auto sr = make_serializer(request);
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push(command::multi);
      sr.push(command::ping, "Some message");
      sr.push(command::incr, "incr-key");
      sr.push_range(command::rpush, "list-key", std::cbegin(list), std::cend(list));
      sr.push(command::lrange, "list-key", 0, -1);
      sr.push(command::exec);
      sr.push(command::quit);
      co_await async_write(socket, buffer(request));

      // Expected responses.
      std::vector<node> exec;

      // Reads the response.
      std::string buffer;
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // hello
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // flushall
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // multi
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // ping
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // incr
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // rpush
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // lrange
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(exec));
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // quit

      // Prints the response.
      std::cout << "General format:\n";
      for (auto const& e: exec) std::cout << e << "\n";

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, nested_response(), net::detached);
   ioc.run();
}
