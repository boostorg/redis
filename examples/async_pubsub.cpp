/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>

/* Implements a coroutine that writes commands in interval and one the
 * reads the commands.
 */

namespace net = aedis::net;
namespace this_coro = net::this_coro;

using namespace aedis;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;
using stimer = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;

net::awaitable<void> publisher(tcp_socket& socket)
{
   auto ex = co_await this_coro::executor;
   try {
      resp::request req;
      req.hello();
      req.subscribe("channel");
      req.subscribe("__keyspace@0__:user:*");

      std::string buffer;
      for (auto i = 0;; ++i) {
	 req.ping();
         req.rpush("user:Marcelo", {i});
	 req.publish("channel", "Some message");

	 co_await async_write(socket, req);
	 req.clear();

	 stimer timer(ex, std::chrono::seconds{2});
	 co_await timer.async_wait();
      }
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}

net::awaitable<void> subscriber()
{
   auto ex = co_await this_coro::executor;
   try {
      tcp::resolver resv(ex);
      auto const r = resv.resolve("127.0.0.1", "6379");
      tcp_socket socket {ex};
      co_await async_connect(socket, r);
      co_spawn(ex, publisher(socket), net::detached);

      std::string buffer;
      for (;;) {
	 resp::response_array<std::string> res;
	 co_await resp::async_read(socket, buffer, res);
	 if (res.is_push())
	    print(res.push().value, "Push");
	 else
	    print(res.result, "Message");
      }
   } catch (std::exception const& e) {
      std::cout << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc {1};
   co_spawn(ioc, subscriber(), net::detached);
   ioc.run();
}

