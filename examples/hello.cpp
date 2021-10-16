/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>

/** A very simple example showing how to
 * 
 *    1. Connect to the redis server.
 *    2. Send a ping
 *    3. and quit.
 *
 *  Notice that in this example we are sending all commands in the same request
 *  instead of waiting the response of each command.
 */

using namespace aedis;

using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using tcp_resolver = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::resolver>;

net::awaitable<tcp_socket> make_connection()
{
   auto ex = co_await net::this_coro::executor;
   tcp_resolver resolver{ex};
   auto const res = co_await resolver.async_resolve("127.0.0.1", "6379");
   tcp_socket socket{ex};
   co_await net::async_connect(socket, res);
   co_return std::move(socket);
}

net::awaitable<void> ping()
{
   auto socket = co_await make_connection();

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().hello();
   requests.back().ping();
   requests.back().quit();

   resp3::consumer cs;
   for (;;) {
      resp3::response resp;
      co_await cs.async_consume(socket, requests, resp);
      std::cout << requests.front().elements.front() << "\n" << resp << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, ping(), net::detached);
   ioc.run();
}
