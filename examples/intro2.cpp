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
using aedis::resp3::response_adapter;

namespace net = aedis::net;
using net::async_write;
using net::buffer;

/* Similar to the basic1 example but
  
     1. Reads the responses in a loop.
     2. Prints the command to which the response belongs to.

   The request class maintains a queue of commands that have been
   added to the request.
 */
net::awaitable<void> ping()
{
   try {
      serializer<command> sr;
      sr.push(command::hello, 3);
      sr.push(command::ping);
      sr.push(command::quit);

      auto socket = co_await connect();
      co_await async_write(socket, buffer(sr.request()));

      std::string buffer;
      while (!std::empty(sr.commands)) {
         std::vector<node> resp;
         auto adapter = response_adapter(&resp);

	 co_await async_read(socket, buffer, adapter);

	 std::cout
	    << sr.commands.front() << "\n"
	    << resp << std::endl;

	 sr.commands.pop();
      }
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

/// \example intro2.cpp
