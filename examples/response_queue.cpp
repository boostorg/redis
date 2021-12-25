/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
/// \example basic1.cpp
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <aedis/aedis.hpp>
#include "utils.ipp"

using aedis::command;
using aedis::resp3::serializer;
using aedis::resp3::async_read;
using aedis::resp3::node;
using aedis::resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;

/* Processes the responses in a loop using the helper queue.
 
   In most cases commands will be added dynamically to the request for
   example as users interact with the code. In order to process the
   responses asynchronously users have to keep a queue of the expected
   commands or use the one provided by the serializer class.

   The example below shows how to do it.
 */
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
               co_await async_read(socket, buffer, adapt(ping));
               break;
            case command::quit:
               co_await async_read(socket, buffer, adapt(quit));
               break;
            default:
               co_await async_read(socket, buffer);
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

/// \example response_queue.cpp
