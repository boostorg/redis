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

/* In previous examples we sent some commands (ping) to redis and
   quit (closed) the connection. In this example we send a
   subscription to a channel and start reading server side messages
   indefinitely.
  
   Notice we store the id of the connection (attributed by the redis
   server) to be able to identify it (in logs for example).
  
   After starting the example you can test it by sending messages with
   the redis-client like this
  
      $ redis-cli -3
      127.0.0.1:6379> PUBLISH channel1 some-message
      (integer) 3
      127.0.0.1:6379>
  
   The messages will then appear on the terminal you are running the
   example.
 */
net::awaitable<void> subscriber()
{
   resp3::request<command> req;
   req.push(command::hello, "3");
   req.push(command::subscribe, "channel1", "channel2");

   auto socket = co_await make_connection("127.0.0.1", "6379");
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

      std::cout
	 << "Subscriber " << id << ":\n"
	 << resp << std::endl;
   }
}

int main()
{
   net::io_context ioc;

   // Starts some subscribers.
   co_spawn(ioc, subscriber(), net::detached);
   co_spawn(ioc, subscriber(), net::detached);
   co_spawn(ioc, subscriber(), net::detached);
   ioc.run();
}

/// \example intro4.cpp
