/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>

#include <stack>
#include <iomanip>

/* Implements a coroutine that writes commands in interval and one the
 * reads the commands.
 */

namespace net = aedis::net;
namespace this_coro = net::this_coro;

using namespace aedis;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;
using stimer = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;

struct receiver {
   void receive(resp::command cmd, resp::type t, std::vector<std::string> v)
   {
      std::cout
	 << std::left << std::setw(20) << resp::to_string(cmd)
	 //<< std::left << std::setw(20) << (int)cmd
	 << std::left << resp::to_string(t)
	 << std::endl;
   }
};

net::awaitable<void>
publisher(tcp_socket& socket, resp::request<resp::event>& req)
{
   auto ex = co_await this_coro::executor;
   try {
      req.hello();
      req.flushall();
      req.subscribe("channel");
      req.subscribe("__keyspace@0__:user:*");
      req.ping();
      req.set("aaaa", {std::to_string(1)});
      req.get("aaaa");
      req.del("aaaa");
      req.rpush("user:Marcelo", {1, 2, 3});
      req.lrange("user:Marcelo");
      req.publish("channel", "Some message");
      req.multi();
      req.lrange("user:Marcelo");
      req.exec();
      req.set("aaaa", {std::to_string(2)});
      req.get("aaaa");
      req.multi();
      req.lrange("user:Marcelo");
      req.ping();
      req.lrange("user:Marcelo");
      req.ping();
      req.lrange("user:Marcelo");
      req.ping();
      req.lrange("user:Marcelo");
      req.exec();
      //req.quit();

      co_await async_write(socket, req);
      stimer timer(ex, std::chrono::seconds{2});
      co_await timer.async_wait();
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
      resp::request req;
      co_spawn(ex, publisher(socket, req), net::detached);

      std::string buffer;
      receiver recv;
      resp::responses resps;
      for (;;) {
	 resp::type type;
	 co_await async_read_type(socket, buffer, type);
	 auto cmd = resp::command::none;
	 if (type != resp::type::push)
	    cmd = req.events.front().first;

	 // The next two ifs are used to deal with pipelines.
	 auto const is_multi = cmd == resp::command::multi;
	 auto const is_exec = cmd == resp::command::exec;
	 auto const trans_empty = std::empty(resps.trans);
	 if (is_multi || (!trans_empty && !is_exec)) {
	    auto const* res = cmd == resp::command::multi ? "OK" : "QUEUED";
	    co_await resp::async_read(socket, buffer, resps.blob_string);
	    assert(resps.blob_string.result == res);
	    resps.trans.push(req.events.front().first);
	    req.events.pop();
	    continue;
	 }

	 if (cmd == resp::command::exec) {
	    assert(resps.trans.front() == resp::command::multi);
	    co_await resp::async_read(socket, buffer, resps.depth1);
	    //assert(std::size(resps.trans) == std::size(resps.depth1));
	    resps.trans.pop(); // Removes multi.
	    int i = 0;
	    while (!std::empty(resps.trans)) {
	       // TODO: type must come from the queue.
	       // TODO: resps.depth1 is wrong.
	       recv.receive(resps.trans.front(), type, {});
	       resps.trans.pop();
	       ++i;
	    }
	    resps.depth1.clear();
	    resps.trans = {};
	    req.events.pop(); // exec
	    continue;
	 }

	 resp::response_array<std::string> array;
	 co_await resp::async_read(socket, buffer, array);
	 recv.receive(cmd, type, std::move(array.result));
	 array.result.clear();

	 if (type != resp::type::push)
	    req.events.pop();

	 if (std::empty(req.events))
	    req.clear();
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

