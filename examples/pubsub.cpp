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

net::awaitable<void> publisher()
{
   auto ex = net::this_coro::executor;
   auto socket = co_await make_connection();

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().push(command::hello, "3");

   resp3::connection conn;
   for (;;) {
      resp3::response resp;
      co_await conn.async_consume(socket, requests, resp);

      if (requests.front().elements.front().cmd == command::hello) {
	 prepare_next(requests);
	 requests.back().publish("channel1", "Message to channel1");
	 requests.back().publish("channel2", "Message to channel2");
	 requests.back().push(command::quit);
      }
   }
}

net::awaitable<void> subscriber()
{
   auto ex = net::this_coro::executor;
   auto socket = co_await make_connection();

   std::string id;

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().push(command::hello, "3");

   resp3::connection conn;
   for (;;) {
      resp3::response resp;
      co_await conn.async_consume(socket, requests, resp);

      if (resp.get_type() == resp3::type::push) {
	 std::cout << "Subscriber " << id << ":\n" << resp << std::endl;
         continue;
      }

      if (requests.front().elements.front().cmd == command::hello) {
	 id = resp.raw().at(8).data;
	 prepare_next(requests);
	 requests.back().subscribe({"channel1", "channel2"});
      }
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
