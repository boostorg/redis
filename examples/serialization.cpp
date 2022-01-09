/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

#include <string>
#include <iostream>
#include <charconv>

#include "utils.ipp"

namespace resp3 = aedis::resp3;
using aedis::command;
using resp3::serializer;
using resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;
using net::dynamic_buffer;

/* Illustrates how to serialize your own data.
   
   Some use cases for this functionality are
  
   1. Improve performance and reduce latency by avoiding copies.
   2. Store json objets in redis.
   3. etc.
  
   Proceedure: Define the to_string and from_string functions for your
   own data types.
 */

// The struct we would like to store in redis using our own
// serialization.
struct mydata {
  int a;
  int b;
};

// Serializes to Tab Separated Value (TSV).
std::string to_string(mydata const& obj)
{
   return std::to_string(obj.a) + '\t' + std::to_string(obj.b);
}

// Deserializes TSV.
void
from_string(
   mydata& obj,
   char const* data,
   std::size_t size,
   std::error_code& ec)
{
   auto const* end = data + size;
   auto const* pos = std::find(data, end, '\t');
   assert(pos != end); // Or use your own error code.

   auto const res1 = std::from_chars(data, pos, obj.a);
   if (res1.ec != std::errc()) {
      ec = std::make_error_code(res1.ec);
      return;
   }

   auto const res2 = std::from_chars(pos + 1, end, obj.b);
   if (res2.ec != std::errc()) {
      ec = std::make_error_code(res2.ec);
      return;
   }
}

net::awaitable<void> serialization()
{
   try {
      auto socket = co_await connect();

      mydata obj{21, 22};

      serializer<command> sr;
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push(command::set, "key", obj);
      sr.push(command::get, "key");
      sr.push(command::quit);
      co_await async_write(socket, buffer(sr.request()));

      // The response.
      mydata get;

      // Reads the responses.
      std::string buffer;
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // hello
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // flushall
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // set
      co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(get));
      co_await resp3::async_read(socket, dynamic_buffer(buffer)); // quit

      // Print the responses.
      std::cout << "get: a = " << get.a << ", b = " << get.b << "\n";

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, serialization(), net::detached);
   ioc.run();
}
