/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>

namespace net = aedis::net;
namespace this_coro = net::this_coro;
using namespace aedis;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;

net::awaitable<void> example1()
{
   try {
      resp::request req;
      req.hello();
      req.set("Password", {"12345"});
      req.get("Password");
      req.quit();

      auto ex = co_await this_coro::executor;
      tcp::resolver resv(ex);
      auto const r = resv.resolve("127.0.0.1", "6379");
      tcp_socket socket {ex};
      co_await async_connect(socket, r);
      co_await async_write(socket, net::buffer(req.payload));

      std::string buffer;
      for (;;) {
	 switch (req.events.front().first) {
	    case resp::command::hello:
	    {
	       resp::response_flat_map<std::string> res;
	       co_await resp::async_read(socket, buffer, res);
	       print(res.result);
	    } break;
	    case resp::command::get:
	    {
	       resp::response_blob_string res;
	       co_await resp::async_read(socket, buffer, res);
	       std::cout << "get: " << res.result << std::endl;
	    } break;
	    default:
	    {
	       resp::response_ignore res;
	       co_await resp::async_read(socket, buffer, res);
	    }
	 }
	 req.events.pop();
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc {1};
   net::co_spawn(ioc, example1(), net::detached);
   ioc.run();
}

