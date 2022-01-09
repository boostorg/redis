/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <aedis/aedis.hpp>
#include "utils.ipp"

namespace resp3 = aedis::resp3;
using aedis::command;
using resp3::serializer;
using resp3::node;
using resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;
using net::dynamic_buffer;

/// Processes the responses in a loop using the helper queue.
net::awaitable<void> ping()
{
   try {
      auto socket = co_await connect();

      serializer<command> sr;
      sr.push(command::hello, 3);
      sr.push(command::ping);
      sr.push(command::quit);
      co_await async_write(socket, buffer(sr.request()));

      // Expected responses
      std::string ping, quit;

      // Reads the responses.
      std::string buffer;
      while (!std::empty(sr.commands)) {
         switch (sr.commands.front()) {
            case command::ping:
               co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(ping));
               break;
            case command::quit:
               co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(quit));
               break;
            default:
               co_await resp3::async_read(socket, dynamic_buffer(buffer));
         }

	 sr.commands.pop();
      }

      // Print the responses.
      std::cout
	 << "Ping: " << ping << "\n"
	 << "Quit: " << quit
	 << std::endl;

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
