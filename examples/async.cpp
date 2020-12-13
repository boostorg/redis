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
      print(res.result);
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
      print(res.result);
   }
}

net::awaitable<void> example3()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, r);

   resp::pipeline p;
   p.flushall();
   p.rpush("key", {1, 2, 3});
   p.sadd("set", std::set<int>{1, 2, 3});
   p.lrange("key");
   p.lrange("key");
   p.lrange("key");

   co_await async_write(socket, buffer(p.payload));

   resp::buffer buffer;

   resp::response res1;
   co_await resp::async_read(socket, buffer, res1);
   co_await resp::async_read(socket, buffer, res1);
   co_await resp::async_read(socket, buffer, res1);

   resp::response_list<int> res2;
   co_await resp::async_read(socket, buffer, res2);
   print(res2.result);

   resp::response_list<long long> res3;
   co_await resp::async_read(socket, buffer, res3);
   print(res3.result);

   resp::response_list<std::string> res5;
   co_await resp::async_read(socket, buffer, res5);
   print(res5.result);

   std::cout << "aaa" << std::endl;
   resp::response_set<int> res6;
   co_await resp::async_read(socket, buffer, res6);
   print(res6.result);
}

int main()
{
   io_context ioc {1};
   co_spawn(ioc, example1(), detached);
   co_spawn(ioc, example2(), detached);
   co_spawn(ioc, example3(), detached);
   ioc.run();
}

