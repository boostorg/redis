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

/* A very simple example to illustrate the basic principles. It adds
 * three commands to the request and reads the response one after the
 * other.
 *
 * Notice the responses are read in the same object for
 * simplification.
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
   resp3::response resp;
   co_await async_read(socket, buffer, resp);
   co_await async_read(socket, buffer, resp);
   co_await async_read(socket, buffer, resp);

   std::cout << resp << std::endl;
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, ping(), net::detached);
   ioc.run();
}
