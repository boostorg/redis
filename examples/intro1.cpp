/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>
#include <iostream>

#include "utils.ipp"

using aedis::command;
using aedis::resp3::request;
using aedis::resp3::response;
using aedis::resp3::async_read;

namespace net = aedis::net;

/* A simple example that illustrates the basic principles. Three
   commands are sent to redis in the same request
  
      1. hello
      2. ping
      3. quit
  
   The responses are then read individually and for simplification in
   the same response object.
*/
net::awaitable<void> ping()
{
   try {
      request req;
      req.push(command::hello, 3);
      req.push(command::ping);
      req.push(command::quit);

      // Helper function form utils.ipp.
      auto socket = co_await make_connection("127.0.0.1", "6379");

      // Writes to the stream (sends to redis)
      co_await async_write(socket, req);

      // Read auxiliary buffer.
      std::string buffer;

      response resp;

      // Reads the response to hello
      co_await async_read(socket, buffer, resp);

      // Reads the response to ping
      co_await async_read(socket, buffer, resp);

      // Reads the response to quit
      co_await async_read(socket, buffer, resp);

      std::cout << resp << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

/// The main function that starts the coroutine.
int main()
{
   net::io_context ioc;
   co_spawn(ioc, ping(), net::detached);
   ioc.run();
}

/// \example intro1.cpp
