/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>

namespace net = aedis::net;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;

namespace this_coro = net::this_coro;

using namespace net;
using namespace aedis;

net::awaitable<void> create_hashes()
{
   std::map<std::string, std::string> map1
   { {{"Name"},      {"Marcelo"}} 
   , {{"Education"}, {"Physics"}}
   , {{"Job"},       {"Programmer"}}
   };

   std::map<std::string, std::string> map2
   { {{"Name"},      {"Lae"}} 
   , {{"Education"}, {"Engineer"}}
   , {{"Job"},       {"Engineer"}}
   };

   std::map<std::string, std::string> map3
   { {{"Name"},      {"Louis"}} 
   , {{"Education"}, {"Nene"}}
   , {{"Job"},       {"Nene"}}
   };

   resp::pipeline p;
   p.flushall();
   p.hset("user:map1", map1);
   p.hset("user:map2", map2);
   p.hset("user:map3", map3);
   p.quit();

   auto ex = co_await this_coro::executor;
   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");
   tcp_socket socket {ex};
   co_await async_connect(socket, r);
   co_await async_write(socket, buffer(p.payload));

   std::string buffer;
   resp::response res;
   for (;;)
      co_await resp::async_read(socket, buffer, res);
}

net::awaitable<void> read_hashes()
{
   resp::pipeline p;
   p.keys("user:*");

   auto ex = co_await this_coro::executor;
   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");
   tcp_socket socket {ex};
   co_await async_connect(socket, r);
   co_await async_write(socket, net::buffer(p.payload));

   std::string buffer;

   resp::response_array<std::string> keys;
   co_await resp::async_read(socket, buffer, keys);
   print(keys.result);

   // Generates the pipeline to retrieve all hashes.
   resp::pipeline pv;
   for (auto const& o : keys.result)
      pv.hvals(o);
   pv.quit();

   co_await async_write(socket, net::buffer(pv.payload));

   for (auto const& key : keys.result) {
      resp::response_array<std::string> value;
      co_await resp::async_read(socket, buffer, value);
      print(value.result);
   }

   resp::response quit;
   co_await resp::async_read(socket, buffer, quit);
}

int main()
{
   io_context ioc {1};
   co_spawn(ioc, create_hashes(), detached);
   ioc.run();
   ioc.restart();
   co_spawn(ioc, read_hashes(), detached);
   ioc.run();
}

