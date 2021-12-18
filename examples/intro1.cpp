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
using aedis::resp3::serializer;
using aedis::resp3::async_read;
using aedis::resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;

/** \brief A simple example that illustrates the basic principles.
 
    We send three commands in the same request and read the responses
    one after the other
  
    1. hello: Must be be the first command after the connection has been
       stablished. We ignore its response here for simplicity, see
       non_flat_response.cpp

    2. ping

    3. incr

    4. quit: Asks the redis server to close the requests after it has been
       processed.
*/
net::awaitable<void> ping()
{
   try {
      auto socket = co_await connect();

      serializer<command> sr;
      sr.push(command::hello, 3);
      sr.push(command::ping);
      sr.push(command::incr, "key");
      sr.push(command::quit);
      co_await async_write(socket, buffer(sr.request()));

      // Expected responses.
      std::string ping, quit;
      int incr;

      // Reads the responses.
      std::string buffer;
      co_await async_read(socket, buffer); // Ignores
      co_await async_read(socket, buffer, adapt(ping));
      co_await async_read(socket, buffer, adapt(incr));
      co_await async_read(socket, buffer, adapt(quit));

      // Print the responses.
      std::cout
	 << "ping: " << ping << "\n"
	 << "incr: " << incr << "\n"
	 << "quit: " << quit
	 << std::endl;

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