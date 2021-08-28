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
         std::cout << "Unexpected.";
   }

   std::cout << "\n";
}

struct receiver {
   std::queue<pipeline>& reqs;
   response_buffers& bufs;

   void operator()(command cmd, resp3::type type) const
   {
      switch (cmd) {
	 case command::hello:
	 {
	    prepare_queue(reqs);
	    reqs.back().flushall();
	    reqs.back().ping();
	 } break;
	 case command::flushall: break;
	 case command::get:
	 {
	    prepare_queue(reqs);
	    reqs.back().quit();
	 } break;
	 case command::incr: break;
	 case command::quit: break;
	 case command::set: break;
	 case command::ping:
	 {
	    prepare_queue(reqs);
	    reqs.back().incr("a");
	    reqs.back().set("b", {"Some string"});
	    reqs.back().get("b");
	 } break;
	 default: {
	    std::cout << "PUSH notification ("  << type << ")" << std::endl;
	 }
      }
      print_helper(cmd, type, bufs);
   }
};

int main()
{
   net::io_context ioc;

   net::ip::tcp::socket socket{ioc};
   net::ip::tcp::resolver resolver{ioc};
   auto const res = resolver.resolve("127.0.0.1", "6379");
   net::connect(socket, res);

   response_buffers bufs;
   std::queue<pipeline> reqs;
   receiver i{reqs, bufs};
   co_spawn(ioc, async_consume(socket, reqs, bufs, std::ref(i)), net::detached);
   ioc.run();
}
