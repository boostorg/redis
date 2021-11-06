/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>

#include <aedis/aedis.hpp>

#include "types.hpp"
#include "utils.ipp"

using namespace aedis;

net::awaitable<void> stl_containers()
{
   // Helper function to get a connected socket.
   auto socket = co_await make_connection();

   std::vector<int> v
      {1, 2, 3, 4, 5, 6};

   std::set<std::string> s
      {"one", "two", "three"};

   std::map<std::string, std::string> m
      { {"key1", "value1"}
      , {"key2", "value2"}
      , {"key3", "value3"}
      };

   std::queue<resp3::request> requests;
   requests.push({});
   requests.back().push(command::hello, 3);
   requests.back().push(command::flushall);
   requests.back().push_range(command::rpush, "vector", std::cbegin(v), std::cend(v));
   requests.back().push_range(command::sadd, "set", std::cbegin(s), std::cend(s));
   requests.back().push_range(command::hset, "map", std::cbegin(m), std::cend(m));

   resp3::stream<tcp_socket> stream{std::move(socket)};
   for (;;) {
      resp3::response resp;
      co_await stream.async_consume(requests, resp);

      // We are not expecting a push from the server.
      assert(resp.get_type() != resp3::type::push);

      auto const cmd = requests.front().commands.front();
      switch (cmd) {
	 case command::rpush:
	 {
	    std::cout
	       << "Length of vector: " << resp.at(0).data
	       << std::endl;

	    // Retrieve the list again.
	    prepare_next(requests);
	    requests.back().push(command::lrange, "vector", 0, -1);
	 } break;
	 case command::sadd:
	 {
	    std::cout
	       << "Number of elements added to set: " << resp.at(0).data
	       << std::endl;

	    // Retrieve the set again.
	    prepare_next(requests);
	    requests.back().push(command::smembers, "set");
	 } break;
	 case command::hset:
	 {
	    std::cout
	       << "Number of fields added to map: " << resp.at(0).data
	       << std::endl;

	    // Retrieve the map again.
	    prepare_next(requests);
	    requests.back().push(command::hgetall, "map");
	 } break;
	 default:
	 {
	    std::cout 
	       << cmd << "\n"
	       << resp
	       << std::endl;
	 }
      }
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, stl_containers(), net::detached);
   ioc.run();
}
