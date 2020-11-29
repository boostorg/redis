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

net::awaitable<void> example1()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, r);

   std::map<std::string, std::string> map
   { {{"Name"},      {"Marcelo"}} 
   , {{"Education"}, {"Physics"}}
   , {{"Job"},       {"Programmer"}}
   };

   resp::pipeline p;
   p.hset("map", map);
   p.hincrby("map", "Age", 40);
   p.hmget("map", {"Name", "Education", "Job"});
   p.quit();

   co_await async_write(socket, buffer(p.payload));

   resp::buffer buffer;
   for (;;) {
      resp::response res;
      co_await resp::async_read(socket, buffer, res);
      resp::print(res.res);
   }
}

net::awaitable<void> example2()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, r);

   resp::pipeline p;
   p.subscribe("channel");

   co_await async_write(socket, buffer(p.payload));
   resp::buffer buffer;
   for (;;) {
      resp::response res;
      co_await resp::async_read(socket, buffer, res);
      resp::print(res.res);
   }
}

net::awaitable<void> example3()
{
   tcp_socket socket {co_await this_coro::executor};

   sentinel_op2<tcp_socket>::config cfg
   { {"127.0.0.1", "26379"}
   , {"mymaster"} 
   , {"master"}
   };

   instance inst;
   co_await async_get_instance2(socket, cfg, inst);
}

int main()
{
   io_context ioc {1};
   co_spawn(ioc, example1(), detached);
   co_spawn(ioc, example2(), detached);
   co_spawn(ioc, example3(), detached);
   ioc.run();
}

