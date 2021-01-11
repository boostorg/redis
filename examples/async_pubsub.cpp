/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>

#include <stack>

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
   void on_hello(std::vector<std::string> v)
      { std::cout << "hello" << std::endl; }
   void on_lrange(std::vector<std::string> v)
      { std::cout << "lrange " << std::size(v) << std::endl; }
   void on_subscribe(std::string& v)
      { std::cout << "subscribe" << std::endl; }
   void on_ping(std::string v)
      { std::cout << "ping" << std::endl; }
   void on_set(std::string v)
      { std::cout << "set " << v << std::endl; }
   void on_flushall(std::string v)
      { std::cout << "flushall " << v << std::endl; }
   void on_get(std::string v)
      { std::cout << "get " << v << std::endl; }
   void on_quit(std::string v)
      { std::cout << "quit " << v << std::endl; }
   void on_rpush(int v)
      { std::cout << "rpush " << v << std::endl; }
   void on_publish(int v)
      { std::cout << "publish" << std::endl; }
   void on_push(std::vector<std::string> v)
      { std::cout << "push" << std::endl; }
   void on_del(int v)
      { std::cout << "del" << std::endl; }
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
      req.quit();

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
	 auto const cmd = req.events.front().first;

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
	    co_await resp::async_read(socket, buffer, resps.depth1);
	    assert(resps.trans.front() == resp::command::multi);
	    resps.trans.pop();
	    for (auto i = 0; i < std::ssize(resps.trans); ++i) {
	       switch (resps.trans.front()) {
		  case resp::command::lrange: recv.on_lrange(std::move(resps.depth1.at(i))); break;
		  default: {assert(false);}
	       }
	       resps.trans.pop();
	    }
	    resps.depth1.clear();
	    assert(std::empty(resps.trans));
	    req.events.pop(); // exec
	    continue;
	 }

	 switch (type) {
	    case resp::type::push:
	    {
	       co_await resp::async_read(socket, buffer, resps.push);
	       recv.on_push(std::move(resps.push.result));
	       resps.push.result.clear();
	    } break;
	    case resp::type::simple_string:
	    {
	       co_await resp::async_read(socket, buffer, resps.simple_string);
	       switch (cmd) {
		  case resp::command::set: recv.on_set(std::move(resps.simple_string.result)); break;
		  case resp::command::ping: recv.on_ping(std::move(resps.simple_string.result)); break;
		  case resp::command::flushall: recv.on_flushall(std::move(resps.simple_string.result)); break;
		  case resp::command::quit: recv.on_quit(std::move(resps.simple_string.result)); break;
	          default: {assert(false);}
	       }
	       resps.simple_string.result.clear();
	    } break;
	    case resp::type::blob_string:
	    {
	       co_await resp::async_read(socket, buffer, resps.blob_string);
	       switch (cmd) {
		  case resp::command::get: recv.on_get(std::move(resps.blob_string.result)); break;
	          default: { assert(false); }
	       }
	       resps.blob_string.result.clear();
	    } break;
	    case resp::type::map:
	    {
	       co_await resp::async_read(socket, buffer, resps.map);
	       switch (cmd) {
		  case resp::command::hello: recv.on_hello(std::move(resps.map.result)); break;
	          default: {assert(false);}
	       }
	       resps.map.result = {};
	    } break;
	    case resp::type::array:
	    {
	       co_await resp::async_read(socket, buffer, resps.array);
	       switch (cmd) {
		  case resp::command::lrange: recv.on_lrange(std::move(resps.array.result)); break;
	          default: { assert(false); }
	       }
	       resps.array.result.clear();
	    } break;
	    case resp::type::set:
	    {
	       co_await resp::async_read(socket, buffer, resps.set);
	       resps.set.result.clear();
	    } break;
	    case resp::type::number:
	    {
	       co_await resp::async_read(socket, buffer, resps.number);
	       switch (cmd) {
	          case resp::command::rpush: recv.on_rpush(resps.number.result); break;
	          case resp::command::publish: recv.on_publish(resps.number.result); break;
	          case resp::command::del: recv.on_del(resps.number.result); break;
	          default: { assert(false); }
	       }
	    } break;
	    default: { assert(false);
	    }
	 }

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

