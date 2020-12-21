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
   resp::pipeline p;
   p.set("Password", {"12345"});
   p.quit();

   auto ex = co_await this_coro::executor;
   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");
   tcp_socket socket {ex};
   co_await async_connect(socket, r);
   co_await async_write(socket, buffer(p.payload));

   std::string buffer;
   for (;;) {
      resp::response_string res;
      co_await resp::async_read(socket, buffer, res);
      std::cout << res.result << std::endl;
   }
}

int main()
{
   io_context ioc {1};
   co_spawn(ioc, example1(), detached);
   ioc.run();
}

