/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>

#include "types.hpp"

aedis::net::awaitable<tcp_socket> make_connection()
{
   auto ex = co_await aedis::net::this_coro::executor;
   tcp_resolver resolver{ex};
   auto const res = co_await resolver.async_resolve("127.0.0.1", "6379");
   tcp_socket socket{ex};
   co_await aedis::net::async_connect(socket, res);
   co_return std::move(socket);
}

void print_command_raw(std::string const& data, int n)
{
  for (int i = 0; i < n; ++i) {
    if (data[i] == '\n') {
      std::cout << "\\n";
      continue;
    }
    if (data[i] == '\r') {
      std::cout << "\\r";
      continue;
    }
    std::cout << data[i];
  }
}

