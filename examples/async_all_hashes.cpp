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

struct foo {
   std::string id {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
   std::string from {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
   std::string nick {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
   std::string avatar {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
   std::string description {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
   std::string location {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
   std::string product {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
   std::string details {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
   std::string values {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
};

auto make_hset_arg(foo const& p)
{
   std::vector<std::pair<std::string, std::string>> v;
   v.push_back({"id", p.id});
   v.push_back({"from", p.from});
   v.push_back({"nick", p.nick});
   v.push_back({"avatar", p.avatar});
   v.push_back({"description", p.description});
   v.push_back({"location", p.location});
   v.push_back({"product", p.product});
   v.push_back({"details", p.details});
   v.push_back({"values", p.values});
   return v;
}

// tcp::resolver::results_type const& r
net::awaitable<void> create_hashes()
{
   std::vector<foo> posts(20000);
   resp::pipeline p;
   p.flushall();
   for (auto i = 0; i < std::ssize(posts); ++i) {
      std::string const name = "posts:" + std::to_string(i);
      p.hset(name, make_hset_arg(posts[i]));
   }
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

net::awaitable<void> read_hashes_coro()
{
   resp::pipeline p;
   p.keys("posts:*");

   auto ex = co_await this_coro::executor;
   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");
   tcp_socket socket {ex};
   co_await async_connect(socket, r);
   co_await async_write(socket, net::buffer(p.payload));

   std::string buffer;

   resp::response_array<std::string> keys;
   co_await resp::async_read(socket, buffer, keys);
   //print(keys.result);

   // Generates the pipeline to retrieve all hashes.
   resp::pipeline pv;
   for (auto const& o : keys.result)
      pv.hvals(o);
   pv.quit();

   co_await async_write(socket, net::buffer(pv.payload));

   for (auto const& key : keys.result) {
      resp::response_array<std::string> value;
      co_await resp::async_read(socket, buffer, value);
      //print(value.result);
   }

   resp::response quit;
   co_await resp::async_read(socket, buffer, quit);
}

void read_hashes(net::io_context& ioc)
{
   resp::pipeline p;
   p.keys("posts:*");

   tcp::resolver resv(ioc);
   auto const r = resv.resolve("127.0.0.1", "6379");
   tcp::socket socket {ioc};
   net::connect(socket, r);
   net::write(socket, net::buffer(p.payload));

   std::string buffer;

   resp::response_array<std::string> keys;
   resp::read(socket, buffer, keys);

   // Generates the pipeline to retrieve all hashes.
   resp::pipeline pv;
   for (auto const& o : keys.result)
      pv.hvals(o);
   pv.quit();

   net::write(socket, net::buffer(pv.payload));

   for (auto const& key : keys.result) {
      resp::response_array<std::string> value;
      resp::read(socket, buffer, value);
   }

   resp::response quit;
   resp::read(socket, buffer, quit);
}

int main()
{
   io_context ioc {1};
   co_spawn(ioc, create_hashes(), detached);
   ioc.run();
   ioc.restart();
   co_spawn(ioc, read_hashes_coro(), detached);
   ioc.run();
   ioc.restart();
   read_hashes(ioc);
}

