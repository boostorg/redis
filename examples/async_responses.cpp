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
      resp::pipeline p;
      p.rpush("list", {1, 2, 3});
      p.lrange("list");
      p.quit();

      auto ex = co_await this_coro::executor;
      tcp::resolver resv(ex);
      tcp_socket socket {ex};
      co_await net::async_connect(socket, resv.resolve("127.0.0.1", "6379"));
      co_await net::async_write(socket, net::buffer(p.payload));

      std::string buffer;
      resp::response_number<int> list_size;
      co_await resp::async_read(socket, buffer, list_size);
      std::cout << list_size.result << std::endl;

      resp::response_list<int> list;
      co_await resp::async_read(socket, buffer, list);
      print(list.result);

      resp::response_simple_string ok;
      co_await resp::async_read(socket, buffer, ok);
      std::cout << ok.result << std::endl;

      resp::response noop;
      co_await resp::async_read(socket, buffer, noop);

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
