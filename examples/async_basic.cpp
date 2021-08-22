/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

using namespace aedis;

void print_helper(command cmd, resp3::type type, response_buffers& bufs)
{
   std::cout << cmd << " (" << type << "): ";

   switch (type) {
      case resp3::type::simple_string: std::cout << bufs.simple_string; break;
      case resp3::type::blob_string: std::cout << bufs.blob_string; break;
      case resp3::type::number: std::cout << bufs.number; break;
      default:
         std::cout << "Unexpected." << "\n";
   }

   std::cout << "\n";
}

net::awaitable<void>
reader(net::ip::tcp::socket& socket, std::queue<pipeline>& reqs)
{
   response_buffers bufs;
   std::string buffer;

   prepare_queue(reqs);
   reqs.back().hello("3");

   co_await async_write(socket, net::buffer(reqs.back().payload), net::use_awaitable);

   detail::response_adapters adapters{bufs};
   for (;;) {
      auto const event = co_await async_consume(socket, buffer, adapters, reqs);

      switch (event.first) {
	 case command::hello:
	 {
	    auto const empty = prepare_queue(reqs);
	    reqs.back().ping();
	    reqs.back().incr("a");
	    reqs.back().set("b", {"Some string"});
	    reqs.back().get("b");
	    reqs.back().quit();
	    if (empty)
	       co_await async_write_all(socket, reqs);
	 } break;
	 case command::get:
	 case command::incr:
	 case command::quit:
	 case command::set:
	 case command::ping:
	    print_helper(event.first, event.second, bufs);
	    break;
	 default: {
	    std::cout << "PUSH notification ("  << event.second << ")" << std::endl;
	 }
      }
   }
}

int main()
{
   net::io_context ioc;

   net::ip::tcp::socket socket{ioc};
   net::ip::tcp::resolver resolver{ioc};
   auto const res = resolver.resolve("127.0.0.1", "6379");
   net::connect(socket, res);

   std::queue<pipeline> reqs;
   co_spawn(ioc, reader(socket, reqs), net::detached);
   ioc.run();
}
