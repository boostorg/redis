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

/* Pushes three commands in a request, write and read them in the same response
 * object.
 */
net::awaitable<void> ping1()
{
   auto socket = co_await make_connection();
   resp3::stream<tcp_socket> stream{std::move(socket)};

   resp3::request req;
   req.push(command::hello, 3);
   req.push(command::ping);
   req.push(command::quit);
   co_await stream.async_write(req);

   resp3::response resp;
   co_await stream.async_read(resp);
   co_await stream.async_read(resp);
   co_await stream.async_read(resp);

   std::cout << resp << std::endl;
}

/* Like obove but uses a while loop to read the commands.
 */
net::awaitable<void> ping2()
{
   auto socket = co_await make_connection();
   resp3::stream<tcp_socket> stream{std::move(socket)};

   resp3::request req;
   req.push(command::hello, 3);
   req.push(command::ping);
   req.push(command::quit);
   co_await stream.async_write(req);

   while (!std::empty(req.commands)) {
      resp3::response resp;
      co_await stream.async_read(resp);
      std::cout << req.commands.front() << ":\n" << resp << std::endl;
      req.commands.pop();
   }
}

void prepare_next(std::queue<resp3::request>& reqs)
{
   if (std::empty(reqs) || std::size(reqs) == 1)
      reqs.push({});
}

/* A more elaborate way of doing what has been done above where we send a new
 * command only after the last one has arrived. This is usually the starting
 * point for more complex applications. Here we also separate the application
 * logic out out the coroutine for clarity.
 */
void process_response3(std::queue<resp3::request>& requests, resp3::response& resp)
{
   std::cout << requests.front().commands.front() << ":\n" << resp << std::endl;

   switch (requests.front().commands.front()) {
      case command::hello:
         prepare_next(requests);
         requests.back().push(command::ping);
         break;
      case command::ping:
         prepare_next(requests);
         requests.back().push(command::quit);
         break;
      default: {};
   }
}

net::awaitable<void> ping3()
{
   auto socket = co_await make_connection();
   resp3::stream<tcp_socket> stream{std::move(socket)};

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().push(command::hello, 3);

   while (!std::empty(requests)) {
      co_await stream.async_write(requests.front());
      while (!std::empty(requests.front().commands)) {
         resp3::response resp;
         co_await stream.async_read(resp);
         process_response3(requests, resp);
         requests.front().commands.pop();
      }

      requests.pop();
   }
}

/* More realistic usage example. Like above but we keep reading from the socket
 * in order to implement a full-duplex communication
 */

net::awaitable<void> ping4()
{
   auto socket = co_await make_connection();
   resp3::stream<tcp_socket> stream{std::move(socket)};

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().push(command::hello, 3);

   for (;;) {
      co_await stream.async_write(requests.front());
      do {
        do {
           resp3::response resp;
           co_await stream.async_read(resp);
           std::cout << requests.front().commands.front() << ":\n" << resp << std::endl;
           requests.front().commands.pop();
        } while (!std::empty(requests.front().commands));
        requests.pop();
      } while (std::empty(requests));
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, ping1(), net::detached);
   co_spawn(ioc, ping2(), net::detached);
   co_spawn(ioc, ping3(), net::detached);
   co_spawn(ioc, ping4(), net::detached);
   ioc.run();
}
