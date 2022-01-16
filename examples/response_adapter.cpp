/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <string_view>

#include <aedis/src.hpp>
#include <aedis/aedis.hpp>
#include "utils.ipp"

namespace resp3 = aedis::resp3;
using aedis::command;
using resp3::type;
using resp3::make_serializer;
using resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;
using net::dynamic_buffer;

/* In the serialization.cpp example we saw how to serialize and
   deserialize Redis responses in user custom types. When serializing
   in custom containers users have to define their own response
   adapter. This example illustrates how to do this.
*/

// An adapter that prints the data it receives in the screen.
struct myadapter {
   void operator()(
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t size,
      std::error_code&)
      {
         std::cout
            << "Type: " << t << "\n"
            << "Aggregate_size: " << aggregate_size << "\n"
            << "Depth: " << depth << "\n"
            << "Data: " << std::string_view(data, size) << "\n"
            << "----------------------" << "\n";
      }
};

net::awaitable<void> adapter_example()
{
   try {
      auto socket = co_await connect();

      auto list = {"one", "two", "three"};

      // Creates and sends the request.
      std::string request;
      auto sr = make_serializer<command>(request);
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push_range(command::rpush, "key", std::cbegin(list), std::cend(list));
      sr.push(command::lrange, "key", 0, -1);
      sr.push(command::quit);
      co_await async_write(socket, buffer(request));

      // Reads the responses.
      std::string rbuffer;
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer)); // hello
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer)); // flushall
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer)); // rpush
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer), myadapter{}); // lrange
      co_await resp3::async_read(socket, dynamic_buffer(rbuffer)); // quit

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, adapter_example(), net::detached);
   ioc.run();
}
