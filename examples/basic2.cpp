/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>

#include "types.hpp"
#include "utils.ipp"

using namespace aedis;

/* Similar to the basic1 example but
 *
 * 1. Reads the response in a loop.
 * 2. Prints the command to which the response belongs.
 */
net::awaitable<void> ping()
{
   auto socket = co_await make_connection();

   resp3::request req;
   req.push(command::hello, 3);
   req.push(command::ping);
   req.push(command::quit);
   co_await async_write(socket, req);

   std::string buffer;
   while (!std::empty(req.commands)) {
      resp3::response resp;
      co_await async_read(socket, buffer, resp);
      std::cout << req.commands.front() << ":\n" << resp << std::endl;
      req.commands.pop();
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, ping(), net::detached);
   ioc.run();
}
