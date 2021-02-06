/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>
#include <aedis/utils.hpp>

namespace net = aedis::net;
namespace this_coro = net::this_coro;
using namespace aedis;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;

enum class myevents
{ ignore
, interesting1
, interesting2
};

net::awaitable<void> example()
{
   try {
      resp::request<myevents> req;
      req.rpush("list", {1, 2, 3});
      req.lrange("list", 0, -1, myevents::interesting1);
      req.sadd("set", std::set<int>{3, 4, 5});
      req.smembers("set", myevents::interesting2);
      req.quit();

      auto ex = co_await this_coro::executor;
      tcp::resolver resv(ex);
      tcp_socket socket {ex};
      co_await net::async_connect(socket, resv.resolve("127.0.0.1", "6379"));
      co_await resp::async_write(socket, req);

      std::string buffer;
      for (;;) {
	 switch (req.events.front().second) {
	    case myevents::interesting1:
	    {
	       resp::response_basic_array<int> res;
	       co_await resp::async_read(socket, buffer, res);
	       resp::print(res.result, "Interesting1");
	    } break;
	    case myevents::interesting2:
	    {
	       resp::response_set<int> res;
	       co_await resp::async_read(socket, buffer, res);
	       resp::print(res.result, "Interesting2");
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
   co_spawn(ioc, example(), net::detached);
   ioc.run();
}

