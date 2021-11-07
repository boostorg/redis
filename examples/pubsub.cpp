/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <chrono>

#include <aedis/aedis.hpp>

#include "utils.ipp"

using namespace aedis;

/** A simple coroutine used to pusblish on a channel and exit.
 */
net::awaitable<void> publisher()
{
   auto socket = co_await make_connection();

   resp3::request req;
   req.push(command::hello, 3);
   req.push(command::publish, "channel1", "Message to channel1");
   req.push(command::publish, "channel2", "Message to channel2");
   req.push(command::quit);
   co_await async_write(socket, req);

   std::string buffer;
   resp3::response_base ignore;
   co_await async_read(socket, buffer, ignore);
   co_await async_read(socket, buffer, ignore);
   co_await async_read(socket, buffer, ignore);
}

net::awaitable<void> subscriber()
{
   auto socket = co_await make_connection();

   resp3::request req;
   req.push(command::hello, "3");
   req.push(command::subscribe, "channel1", "channel2");
   co_await async_write(socket, req);

   std::string buffer;
   resp3::response resp;

   // Reads the response to the hello command.
   co_await async_read(socket, buffer, resp);

   // Saves the id of this connection.
   auto const id = resp.raw().at(8).data;

   // Reads the response to the subscribe command.
   co_await async_read(socket, buffer, resp);

   // Loops to receive server pushes.
   for (;;) {
      resp.clear();
      co_await async_read(socket, buffer, resp);
      std::cout << "Subscriber " << id << ":\n" << resp << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, subscriber(), net::detached);
   co_spawn(ioc, subscriber(), net::detached);
   co_spawn(ioc, subscriber(), net::detached);
   co_spawn(ioc, publisher(), net::detached);
   ioc.run();
}
