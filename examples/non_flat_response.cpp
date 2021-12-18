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
using aedis::resp3::node;

namespace net = aedis::net;
using net::async_write;
using net::buffer;

/** \brief Shows how to read non-flat responses.
 
*/
net::awaitable<void> ping()
{
   try {
      auto socket = co_await connect();

      serializer<command> sr;
      sr.push(command::hello, 3);
      co_await async_write(socket, buffer(sr.request()));

      // Expected response.
      std::vector<node> hello;

      // Reads the response.
      std::string buffer;
      co_await async_read(socket, buffer, adapt(hello));

      // Print the responses.
      std::cout << "hello: ";
      for (auto const& e: hello) std::cout << e << " ";
      std::cout << "\n";

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
