/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
/// \example basic1.cpp
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>

#include "types.hpp"
#include "utils.ipp"

using aedis::command;
using aedis::resp3::request;
using aedis::resp3::response;
using aedis::resp3::async_read;

namespace net = aedis::net;

/* Similar to the basic1 example but
 *
 * 1. Reads the responses in a loop.
 * 2. Prints the command to which the response belongs to.
 */
net::awaitable<void> ping()
{
   try {
      request req;
      req.push(command::hello, 3);
      req.push(command::ping);
      req.push(command::quit);

      auto socket = co_await make_connection("127.0.0.1", "6379");
      co_await async_write(socket, req);

      std::string buffer;
      while (!std::empty(req.commands)) {
	 response resp;
	 co_await async_read(socket, buffer, resp);

	 std::cout
	    << req.commands.front() << "\n"
	    << resp << std::endl;

	 req.commands.pop();
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

/// \example basic2.cpp
