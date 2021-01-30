/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>

#include <stack>

namespace net = aedis::net;
using namespace aedis;
using tcp = net::ip::tcp;

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

std::ostream&
operator<<(std::ostream& os, myevent e)
{
   os << to_string(e);
   return os;
}

auto fill_req(resp::request<myevent>& req)
{
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
   req.set("eee", {std::to_string(8)});
   req.get("eee");
   req.del("eee");
}

net::awaitable<void> subscriber()
{
   auto ex = co_await net::this_coro::executor;
   try {
      tcp::resolver resv(ex);
      auto const r = resv.resolve("127.0.0.1", "6379");
      tcp::socket socket {ex};
      co_await async_connect(socket, r, net::use_awaitable);

      auto reqs = resp::make_request_queue<myevent>();
      resp::response_buffers_ignore resps;
      resp::receiver_base recv;
      net::steady_timer st{ex};

      co_spawn(
	 ex,
	 resp::async_reader(socket, reqs, recv, resps),
	 net::detached);

      resp::async_writer(socket, reqs, st, net::detached);

      auto filler = [](auto& req){fill_req(req);};

      for (;;) {
	 queue_writer(reqs, filler, st);
	 net::steady_timer timer(ex, std::chrono::milliseconds{100});
	 co_await timer.async_wait(net::use_awaitable);
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

