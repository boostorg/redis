/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <chrono>
#include <optional>
#include <aedis/aedis.hpp>

#include "utils.ipp"

using aedis::command;
using aedis::resp3::serializer;
using aedis::resp3::async_read;
using aedis::resp3::adapt;
using aedis::resp3::node;

namespace net = aedis::net;
using net::async_write;
using net::buffer;

/* Shows how to deal with keys that may not exist.
  
   When accessing a key that does not exist, for example due to
   expiration, redis will return null. Aedis supports these usecases
   through std::optional.
 */

net::awaitable<void> key_expiration()
{
   try {
      auto socket = co_await connect();

      // Creates and sends the first request.
      serializer<command> sr;
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push(command::set, "key", "Some payload", "EX", "2");
      sr.push(command::get, "key");
      co_await async_write(socket, buffer(sr.request()));

      // Will hold the response to get.
      std::optional<std::string> get;

      // Reads the responses.
      std::string aux_buffer;
      co_await async_read(socket, aux_buffer); // hello
      co_await async_read(socket, aux_buffer); // flushall
      co_await async_read(socket, aux_buffer);
      co_await async_read(socket, aux_buffer, adapt(get));

      std::cout
        << "Before expiration: " << get.has_value() << ", "
        << *get << "\n";

      // Waits some seconds for the key to expire.
      timer tm{socket.get_executor(), std::chrono::seconds{3}};
      co_await tm.async_wait();

      // Creates and sends the second request, after expiration.
      get.reset(); sr.clear();
      sr.push(command::get, "key");
      sr.push(command::quit);
      co_await async_write(socket, buffer(sr.request()));

      // Reads the response to the second request.
      co_await async_read(socket, aux_buffer, adapt(get));
      co_await async_read(socket, aux_buffer);

      std::cout << "After expiration: " << get.has_value() << "\n";

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, key_expiration(), net::detached);
   ioc.run();
}

/// \example key_expiration.cpp
