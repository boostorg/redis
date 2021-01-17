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

template <class Event>
struct resp_id {
   resp::command cmd;
   resp::type type;
   Event ev;
};

template <class Event>
struct responses {
   resp::response_simple_string<char> simple_string;
   resp::response_blob_string<char> blob_string;
   resp::response_array<std::string> array;
   resp::response_transaction resp_trans;
   std::queue<resp_id<Event>> trans;
};

enum class myevent {zero, one, two, ignore};

#define EXPAND_MYEVENT_CASE(x) case myevent::x: return #x

inline
auto to_string(myevent t)
{
   switch (t) {
      EXPAND_MYEVENT_CASE(zero);
      EXPAND_MYEVENT_CASE(one);
      EXPAND_MYEVENT_CASE(two);
      EXPAND_MYEVENT_CASE(ignore);
      default: assert(false);
   }
}

using event_type = myevent;

struct receiver {
   void receive(resp_id<myevent> const& id, std::vector<std::string> v)
   {
      std::cout
	 << std::left << std::setw(15) << resp::to_string(id.cmd)
	 << std::left << std::setw(20) << resp::to_string(id.type)
	 << std::left << std::setw(20) << to_string(id.ev)
	 << v.back()
	 << std::endl;
   }
};

void fill_request(resp::request<event_type>& req)
{
   req.hello();
   req.flushall();
   req.subscribe("channel");
   req.subscribe("__keyspace@0__:user:*");
   req.ping(myevent::one);
   req.set("aaaa", {std::to_string(1)});
   req.get("aaaa");
   req.del("aaaa");
   req.rpush("user:Marcelo", {1, 2, 3}, myevent::two);
   req.lrange("user:Marcelo");
   req.publish("channel", "Some message");
   req.multi();
   req.lrange("user:Marcelo", 0, -1, myevent::zero);
   req.exec();
   req.set("aaaa", {std::to_string(2)});
   req.get("aaaa");
   req.multi();
   req.lrange("user:Marcelo");
   req.ping();
   req.lrange("user:Marcelo", 0, -1, myevent::zero);
   req.ping();
   req.lrange("user:Marcelo");
   req.ping();
   req.lrange("user:Marcelo");
   req.lrange("user:Marcelo");
   req.exec();
   //req.set("eee", {std::to_string(8)});
   //req.get("eee");
   //req.del("eee");
}

// A coroutine that adds commands to the request continously
net::awaitable<void>
filler(
   std::queue<resp::request<event_type>>& reqs,
   net::steady_timer& trigger)
{
   auto ex = co_await this_coro::executor;
   try {
      for (;;) {
	 std::cout << "filling" << std::endl;
	 resp::request<event_type> req;
	 fill_request(req);
	 auto const empty = std::empty(reqs);
	 reqs.push(req);
	 if (empty) {
	    std::cout << "filler triggering" << std::endl;
	    trigger.cancel();
	 }

	 std::cout << "filler waiting" << std::endl;
	 stimer timer(ex, std::chrono::milliseconds{20});
	 co_await timer.async_wait();
	 std::cout << "filler waiting end" << std::endl;
      }
   } catch (std::exception const& e) {
      std::cerr << "filler: " << e.what() << std::endl;
   }
}

// A coroutine that will write requests to redis.
net::awaitable<void>
publisher(
   tcp_socket& socket,
   net::steady_timer& trigger,
   std::queue<resp::request<event_type>>& reqs)
{
   auto ex = co_await this_coro::executor;
   for (;;) {
      if (!std::empty(reqs)) {
	 std::cout << "Writing ..." << std::endl;
         assert(!std::empty(reqs.front()));
	 co_await async_write(socket, reqs.front());
      }

      std::cout << "Waiting to write ..." << std::endl;
      trigger.expires_after(std::chrono::years{2});
      boost::system::error_code ec;
      co_await trigger.async_wait(net::redirect_error(net::use_awaitable, ec));
      if (!socket.is_open()) {
	 std::cout << "aaa" << std::endl;
	 co_return;
      }
      if (ec == net::error::operation_aborted) {
      } else {
	 std::cout << "Publisher leaving." << std::endl;
	 co_return;
      }
   }
}

net::awaitable<void> subscriber()
{
   auto ex = co_await this_coro::executor;
   try {
      net::steady_timer trigger {ex};
      tcp::resolver resv(ex);
      auto const r = resv.resolve("127.0.0.1", "6379");
      tcp_socket socket {ex};
      co_await async_connect(socket, r);
      std::queue<resp::request<event_type>> reqs;
      co_spawn(ex, publisher(socket, trigger, reqs), net::detached);
      co_spawn(ex, filler(reqs, trigger), net::detached);

      std::string buffer;
      receiver recv;
      responses<event_type> resps;
      for (;;) {
	 auto& req = reqs.front();
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
	    resps.trans.push({req.events.front().first, resp::type::invalid, req.events.front().second});
	    req.events.pop();
	    continue;
	 }

	 if (cmd == resp::command::exec) {
	    assert(resps.trans.front().cmd == resp::command::multi);
	    co_await resp::async_read(socket, buffer, resps.resp_trans);
	    resps.trans.pop(); // Removes multi.
	    int i = 0;
	    while (!std::empty(resps.trans)) {
	       resps.trans.front().type = resps.resp_trans.at(i).t;
	       recv.receive(resps.trans.front(), resps.resp_trans.at(i).value);
	       resps.trans.pop();
	       ++i;
	    }
	    resps.resp_trans.clear();
	    resps.trans = {};
	    req.events.pop(); // exec
	    if (std::empty(req.events)) {
	       std::cout << "reader ready ..." << std::endl;
	       reqs.pop();
	       if (!std::empty(reqs)) {
		  std::cout << "reader triggering ..." << std::endl;
		  trigger.cancel();
	       }
	    }
	    continue;
	 }

	 resp::response_array<std::string> array;
	 co_await resp::async_read(socket, buffer, array);
	 recv.receive({cmd, type, req.events.front().second}, std::move(array.result));
	 array.result.clear();

	 if (type != resp::type::push)
	    req.events.pop();

	 if (std::empty(req.events)) {
	    std::cout << "reader ready ..." << std::endl;
	    reqs.pop();
	    if (!std::empty(reqs)) {
	       std::cout << "reader triggering ..." << std::endl;
	       trigger.cancel();
	    }
	 }
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

