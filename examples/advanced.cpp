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

/* A more elaborate way of doing what has been done above where we send a new
 * command only after the last one has arrived. This is usually the starting
 * point for more complex applications. Here we also separate the application
 * logic out out the coroutine for clarity.
 */
bool prepare_next(std::queue<resp3::request>& reqs)
{
   if (std::empty(reqs)) {
      reqs.push({});
      return true;
   }

   if (std::size(reqs) == 1) {
      reqs.push({});
      return false;
   }

   return false;
}

net::awaitable<void>
writer(tcp_socket& socket, std::queue<resp3::request>& reqs, std::string message)
{
   auto ex = co_await aedis::net::this_coro::executor;
   net::steady_timer t{ex};

   while (socket.is_open()) {
      t.expires_after(std::chrono::milliseconds{100});
      co_await t.async_wait(net::use_awaitable);

      auto const can_write = prepare_next(reqs);
      reqs.back().push(command::publish, "channel", message);
      reqs.back().push(command::publish, "channel", message);
      reqs.back().push(command::publish, "channel", message);
      if (can_write)
	 co_await async_write(socket, reqs.front());
   }
}

net::awaitable<void> reader(tcp_socket& socket, std::queue<resp3::request>& reqs)
{
   // Writes and reads continuosly from the socket.
   for (std::string buffer;;) {
      // Writes the first request in queue and all subsequent
      // ones that have no response e.g. subscribe.
      do {
	 co_await async_write(socket, reqs.front());

         // Pops the request if no response is expected.
	 if (std::empty(reqs.front().commands))
	    reqs.pop();

      } while (!std::empty(reqs) && std::empty(reqs.front().commands));

      // Keeps reading while there is no messages queued waiting to be sent.
      do {
         // Loops to consume the response to all commands in the request.
	 do {
            // Reads the type of the incoming response.
            auto const t = co_await resp3::async_read_type(socket, buffer);

	    if (t == resp3::type::push) {
               resp3::response resp;
               co_await async_read(socket, buffer, resp);
	       std::cout << resp << std::endl;
	    } else {
	       // Prints the command and the response to it.
	       switch (reqs.front().commands.front()) {
		  case command::hello:
		  {
                     resp3::response resp;
                     co_await async_read(socket, buffer, resp);

		     for (auto i = 0; i < 100; ++i) {
			std::string msg = "Writer ";
			msg += std::to_string(i);
			co_spawn(socket.get_executor(), writer(socket, reqs, msg), net::detached);
		     }
		  } break;
		  default:
                  {
                     resp3::response resp;
                     co_await async_read(socket, buffer, resp);

                     std::cout
                        << reqs.front().commands.front() << ":\n"
                        << resp << std::endl;
                  }
	       }
	       // Done with this command, pop.
	       reqs.front().commands.pop();
	    }
	 } while (!std::empty(reqs) && !std::empty(reqs.front().commands));

	 // We may exit the loop above either because we are done
	 // with the response or because we received a server push
	 // while the queue was empty.
	 if (!std::empty(reqs))
	    reqs.pop();

      } while (std::empty(reqs));
   }
}

net::awaitable<void> advanced()
{
   auto socket = co_await make_connection();

   std::queue<resp3::request> reqs;
   reqs.push({});
   reqs.back().push(command::hello, 3);
   reqs.back().push(command::subscribe, "channel");

   co_await co_spawn(socket.get_executor(), reader(socket, reqs), net::use_awaitable);
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, advanced(), net::detached);
   ioc.run();
}
