/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <iostream>
#include <charconv>

#include <aedis/aedis.hpp>

#include "types.hpp"
#include "utils.ipp"

using aedis::command;
using aedis::resp3::type;
using aedis::resp3::serializer;
using aedis::resp3::async_read;
using aedis::resp3::node;
using aedis::resp3::response_adapter;
using aedis::resp3::adapter_ignore;
using aedis::resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;

/* Illustrates how to write a custom response.  Useful to users
 *  seeking to improve performance and reduce latency.
 */

/* This coroutine avoids reading the response to a get command in a
   temporary buffer by using a custom response. This is always
   possible when the application knows the data type being stored in a
   specific key.
 */
net::awaitable<void> example()
{
   try {
      serializer<command> sr;
      sr.push(command::hello, 3);

      sr.push(command::set, "key", 42);
      sr.push(command::get, "key");
      sr.push(command::quit);

      auto socket = co_await connect();
      co_await async_write(socket, buffer(sr.request()));

      std::string read_buffer;

      co_await async_read(socket, read_buffer); // hello
      co_await async_read(socket, read_buffer); // set

      int value;
      co_await async_read(socket, read_buffer, adapt(value)); // get

      std::cout << value << std::endl;

      // quit.
      co_await async_read(socket, read_buffer);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, example(), net::detached);
   ioc.run();
}

/// \example custom_response1.cpp
