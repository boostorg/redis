/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <sstream>

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>

namespace net = aedis::net;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;

namespace this_coro = net::this_coro;

struct user {
   std::string name;
   int age;
   int height;
   int weight;

   operator std::string() const
   {
      std::stringstream ss;
      ss << name << ";" << age << ";" << height << ";" << weight;
      return ss.str();
   }
};

using namespace net;
using namespace aedis;

net::awaitable<void> example1()
{
   std::list<user> users
   { {"Louis", 1, 2, 10}
   , {"Marcelo", 10, 20, 10}
   };

   resp::pipeline p;
   p.flushall();
   p.rpush("kabuff", users);
   p.lrange("kabuff");
   p.quit();

   auto ex = co_await this_coro::executor;
   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");
   tcp_socket socket {ex};
   co_await async_connect(socket, r);
   co_await async_write(socket, buffer(p.payload));

   std::string buffer;
   for (;;) {
      resp::response_array<std::string> res;
      co_await resp::async_read(socket, buffer, res);
      print(res.result);
   }
}

int main()
{
   io_context ioc {1};
   co_spawn(ioc, example1(), detached);
   ioc.run();
}

