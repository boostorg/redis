/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <chrono>
#include <aedis/aedis.hpp>
#include "utils.ipp"

namespace resp3 = aedis::resp3;
using aedis::command;
using resp3::serializer;
using resp3::adapt;
using resp3::node;

namespace net = aedis::net;
using net::async_write;
using net::buffer;
using net::dynamic_buffer;

/* In previous examples we sent some commands (ping) to redis and
   quit (closed) the connection. In this example we send a
   subscription to a channel and start reading server side messages
   indefinitely.
  
   Notice we store the id of the connection (attributed by the redis
   server) to be able to identify it (in the logs for example).
  
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
   auto socket = co_await connect();

   serializer<command> sr;
   sr.push(command::hello, "3");
   sr.push(command::subscribe, "channel1", "channel2");
   co_await async_write(socket, buffer(sr.request()));

   std::vector<node> resp;

   // Reads the response to the hello command.
   std::string buffer;
   co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(resp));
   co_await resp3::async_read(socket, dynamic_buffer(buffer));

   // Saves the id of this connection.
   auto const id = resp.at(8).data;

   // Loops to receive server pushes.
   for (;;) {
      resp.clear();
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(resp));

      std::cout
	 << "Subscriber " << id << ":\n"
	 << resp << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, subscriber(), net::detached);
   co_spawn(ioc, subscriber(), net::detached);
   co_spawn(ioc, subscriber(), net::detached);
   ioc.run();
}
