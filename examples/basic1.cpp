/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <iostream>

#include <aedis/aedis.hpp>

#include "types.hpp"
#include "utils.ipp"

using namespace aedis;

/** A simple example that illustrates the basic principles. Three commands are
 *  sent in the same request
 *
 *     1. hello (always required)
 *     2. ping
 *     3. quit
 *
 *  The responses are then read in sequence. For simplification we read all
 *  responses on the same object.
 */
net::awaitable<void> ping()
{
   try {
      resp3::request req;
      req.push(command::hello, 3);
      req.push(command::ping);
      req.push(command::quit);

      auto socket = co_await make_connection();
      co_await async_write(socket, req);

      std::string buffer;
      resp3::response resp;

      // hello
      co_await async_read(socket, buffer, resp);

      // ping
      co_await async_read(socket, buffer, resp);

      // quit
      co_await async_read(socket, buffer, resp);

      std::cout << resp << std::endl;

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
