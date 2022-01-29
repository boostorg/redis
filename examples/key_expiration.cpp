/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <chrono>
#include <optional>

#include <aedis/src.hpp>
#include <aedis/aedis.hpp>

#include "lib/net_utils.hpp"

namespace resp3 = aedis::resp3;
using aedis::redis::command;
using aedis::redis::make_serializer;
using resp3::adapt;
using resp3::node;

namespace net = aedis::net;
using net::async_write;
using net::buffer;
using net::dynamic_buffer;

net::awaitable<void> key_expiration()
{
   try {
      auto socket = co_await connect();

      // Creates and sends the first request.
      std::string request;
      auto sr = make_serializer(request);
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push(command::set, "key", "Some payload", "EX", "2");
      sr.push(command::get, "key");
      co_await async_write(socket, buffer(request));

      // Will hold the response to get.
      std::optional<std::string> get;

      // Reads the responses.
      std::string rbuffer;
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer)); // hello
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer)); // flushall
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer)); // set
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer), adapt(get));

      std::cout
        << "Before expiration: " << get.has_value() << ", "
        << *get << std::endl;

      // Waits some seconds for the key to expire.
      timer tm{socket.get_executor(), std::chrono::seconds{3}};
      co_await tm.async_wait();

      // Creates a request to get after expiration.
      get.reset(); request.clear();
      sr.push(command::get, "key");
      sr.push(command::get, "key");
      sr.push(command::quit);
      co_await async_write(socket, buffer(request));

      // Reads the response to the second request.
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer), adapt(get));

      // Reading without an optional will result in an error.
      std::string str;
      boost::system::error_code ec;
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer),
	          adapt(str), net::redirect_error(net::use_awaitable, ec));

      // Quit
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer));

      std::cout << "After expiration (optional): " << get.has_value() << "\n";
      std::cout << "After expiration (non-optional): " << ec.message() << "\n";

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, key_expiration(), net::detached);
   ioc.run();
}
