/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/src.hpp>
#include <aedis/aedis.hpp>
#include "utils.ipp"

namespace resp3 = aedis::resp3;
using aedis::command;
using resp3::make_serializer;
using resp3::adapt;
using resp3::node;

namespace net = aedis::net;
using net::async_write;
using net::buffer;
using net::dynamic_buffer;

/// Shows how to read nested responses.

net::awaitable<void> nested_response()
{
   try {
      auto socket = co_await connect();

      std::string request;
      auto sr = make_serializer<command>(request);
      sr.push(command::hello, 3);
      sr.push(command::quit);
      co_await async_write(socket, buffer(request));

      // Expected responses.
      node ping;
      std::vector<node> hello;

      // Reads the response.
      std::string buffer;
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(hello));
      co_await resp3::async_read(socket, dynamic_buffer(buffer));

      // Prints the response.
      for (auto const& e: hello) std::cout << e << "\n";

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
