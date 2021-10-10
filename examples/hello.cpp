/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>

/* A very simple example where we connect to redis and quit.
 */

using namespace aedis;

using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;

void print_event(std::pair<command, std::string> const& p)
{
   std::cout << "Event: " << p.first << ".";

   if (!std::empty(p.second))
      std::cout << " Key: " << p.second << ".";

   std::cout << std::endl;
}

net::awaitable<void>
example(
   tcp_socket& socket,
   std::queue<resp3::request>& requests)
{
   requests.push({});
   requests.back().hello("3");

   resp3::response resp;
   resp3::consumer cs;

   for (;;) {
      co_await cs.async_consume(socket, requests, resp);
      auto const cmd = requests.front().ids.front().first;
      auto const type = requests.front().ids.front().first;

      std::cout << cmd << "\n" << resp << std::endl;

      if (cmd == command::hello) {
	 prepare_next(requests);
	 requests.back().quit();
      }

      resp.clear();
   }
}

int main()
{
   net::io_context ioc;
   net::ip::tcp::resolver resolver{ioc};
   auto const res = resolver.resolve("127.0.0.1", "6379");

   tcp_socket socket{ioc};
   net::connect(socket, res);

   std::queue<resp3::request> requests;
   co_spawn(ioc, example(socket, requests), net::detached);
   ioc.run();
}
