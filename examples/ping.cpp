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

net::awaitable<void> ping()
{
   // Helper function to get a connected socket.
   auto socket = co_await make_connection();

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().push(command::hello, 3);
   requests.back().push(command::ping);
   requests.back().push(command::quit);

   resp3::stream<tcp_socket> stream{std::move(socket)};
   for (;;) {
      resp3::response resp;
      co_await stream.async_consume(requests, resp);

      std::cout
	 << requests.front().commands.front() << "\n"
	 << resp << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, ping(), net::detached);
   ioc.run();
}