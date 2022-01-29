/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

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

net::awaitable<void> ping()
{
   try {
      auto socket = co_await connect(); // See lib/net_utils.hpp

      // Creates and sends the request.
      std::string request;
      auto sr = make_serializer(request);
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push(command::ping);
      sr.push(command::incr, "key");
      sr.push(command::quit);
      co_await async_write(socket, buffer(request));

      // Expected responses.
      int incr;
      std::string ping;

      // Reads the responses.
      std::string buffer;
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // hello (ignored)
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // flushall
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(ping));
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(incr));
      co_await resp3::async_read(socket, dynamic_buffer(buffer));

      // Print the responses.
      std::cout
	 << "ping: " << ping << "\n"
	 << "incr: " << incr << "\n";

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, ping(), net::detached);
   ioc.run();
}
