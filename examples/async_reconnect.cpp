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
using stimer = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;

namespace this_coro = net::this_coro;

using namespace net;
using namespace aedis;

net::awaitable<void> example1()
{
   auto ex = co_await this_coro::executor;
   for (;;) {
      try {
	 resp::request p;
	 p.set("Password", {"12345"});
	 p.quit();

	 tcp::resolver resv(ex);
	 auto const r = resv.resolve("127.0.0.1", "6379");
	 tcp_socket socket {ex};
	 co_await async_connect(socket, r);
	 co_await async_write(socket, buffer(p.payload));

	 std::string buffer;
	 for (;;) {
	    resp::response_simple_string res;
	    co_await resp::async_read(socket, buffer, res);
	    std::cout << res.result << std::endl;
	 }
      } catch (std::exception const& e) {
	 std::cerr << "Error: " << e.what() << std::endl;
	 stimer timer(ex, std::chrono::seconds{2});
	 co_await timer.async_wait();
      }
   }
}

int main()
{
   io_context ioc {1};
   co_spawn(ioc, example1(), detached);
   ioc.run();
}

