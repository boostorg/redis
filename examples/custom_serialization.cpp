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
using aedis::resp3::serializer;
using aedis::resp3::async_read;
using aedis::resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;

/** \brief Illustrates how to serialize your own data.
 *
 *   Some use cases for this functionality are
 *
 *   1. Improve performance and reduce latency.
 *   2. Store json objets in redis.
 *   3. etc.
 *
 *   Define the to_string and from_string for your own data types.
 */

struct mydata {
  int field1 = 22;
  std::string field2 = "field2";
};

std::string to_string(mydata const& o)
{
   // Not implemented, only as example.
   return "The serialization string.";
}

void from_string(mydata& o, char const* data, std::size_t size)
{
  // Not implemented, only as example.
  o.field1 = 22;
  o.field2 = "field2";
}

std::string make_request()
{
   mydata data;
   std::vector<mydata> vec(10);

   serializer<command> sr;
   sr.push(command::hello, 3);
   sr.push(command::set, "key1", data);
   sr.push(command::get, "key1");
   sr.push_range(command::rpush, "key2", std::cbegin(vec), std::cend(vec));
   sr.push(command::lrange, "key2", 0, -1);
   sr.push(command::quit);

   return sr.request();
}

net::awaitable<void> example()
{
   try {
      auto socket = co_await connect();

      // Creates the request and send it to redis.
      auto const req = make_request();
      co_await async_write(socket, buffer(req));

      // The responses.
      mydata get;
      int rpush;
      std::vector<mydata> lrange;

      // Reads the responses.
      std::string buffer;
      co_await async_read(socket, buffer); // hello
      co_await async_read(socket, buffer); // set
      co_await async_read(socket, buffer, adapt(get)); // get
      co_await async_read(socket, buffer, adapt(rpush)); // rpush
      co_await async_read(socket, buffer, adapt(lrange)); // lrange
      co_await async_read(socket, buffer); // quit

      // Print the responses.
      std::cout
	 << "get: " << get.field1 << " " << get.field2 << "\n"
	 << "rpush: " << rpush << "\n";

      std::cout << "lrange: ";
      for (auto const& e: lrange) std::cout << e.field1 << " " << e.field2 << " ";
      std::cout << std::endl;

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

/// \example custom_serialization.cpp
