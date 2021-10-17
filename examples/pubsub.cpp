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

net::awaitable<void>
publisher(tcp_socket& socket, std::queue<resp3::request>& requests)
{
   auto ex = net::this_coro::executor;
   //timer st(ex);

   for (auto i = 0; i < 4; ++i) {
      if (!socket.is_open())
         co_return;

      prepare_next(requests);
      requests.back().publish("channel1", "Message to channel1");
      requests.back().publish("channel2", "Message to channel2");
      //st.expires_after(std::chrono::seconds{1});
      //co_await st.async_wait();
      co_await resp3::detail::async_write_some(socket, requests);
   }
}

net::awaitable<void> subscriber()
{
   auto ex = net::this_coro::executor;
   auto socket = co_await make_connection();

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().hello();

   resp3::consumer cs;
   for (;;) {
      resp3::response resp;
      co_await cs.async_consume(socket, requests, resp);

      if (resp.get_type() == resp3::type::push) {
	 std::cout << "Received a server push\n" << resp << std::endl;
         continue;
      }

      if (requests.front().elements.front().cmd == command::hello) {
	 prepare_next(requests);
	 requests.back().subscribe({"channel1", "channel2"});
      }
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, subscriber(), net::detached);
   ioc.run();
}
