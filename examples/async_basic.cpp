/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

namespace net = aedis::net;

namespace net = aedis::net;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;

namespace this_coro = net::this_coro;

using namespace net;
using namespace aedis;

net::awaitable<void> example()
{
   try {
      auto ex = co_await this_coro::executor;

      resp::request p;
      p.multi();
      p.hello();
      p.rpush("list", {1, 2, 3});
      p.lrange("list");
      p.exec();
      p.quit();

      tcp::resolver resv(ex);
      tcp_socket socket {ex};
      co_await net::async_connect(socket, resv.resolve("127.0.0.1", "6379"));
      co_await net::async_write(socket, net::buffer(p.payload));

      std::string buffer;
      for (;;) {
	 resp::response_array<std::string> hello;
	 co_await resp::async_read(socket, buffer, hello);
	 print(hello.result);
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

int main()
{
   io_context ioc {1};
   co_spawn(ioc, example(), detached);
   ioc.run();
}

