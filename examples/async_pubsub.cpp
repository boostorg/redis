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
namespace this_coro = net::this_coro;

using namespace aedis;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;
using stimer = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;

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

struct myreceiver : public resp::receiver_base<myevent>
{
   using event_type = myevent;
   //void receive(
   //   resp::response_id<event_type> const& id,
   //   std::vector<std::string> v) override final
   //{
   //   std::cout << id << ": " << v.back() << std::endl;
   //}
};

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

// A coroutine that adds commands to the request continously
net::awaitable<void>
filler(
   std::queue<resp::request<myevent>>& reqs,
   net::steady_timer& st)
{
   try {
      auto ex = co_await this_coro::executor;
      auto filler = [](auto& req){fill_req(req);};
      for (;;) {
	 queue_writer(reqs, filler, st);
	 stimer timer(ex, std::chrono::milliseconds{1000});
	 co_await timer.async_wait();
      }
   } catch (std::exception const& e) {
      std::cerr << "filler: " << e.what() << std::endl;
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
      net::steady_timer st{ex};

      std::queue<resp::request<myevent>> reqs;
      reqs.push({});
      reqs.front().hello();

      myreceiver recv;

      co_spawn(
	 ex,
	 resp::async_read_responses(socket, reqs, recv),
	 net::detached);

      co_spawn(
	 ex,
	 resp::async_writer(socket, reqs, st),
	 net::detached);

      // Starts some fillers.
      co_spawn(
	 ex,
	 filler(reqs, st),
	 net::detached);

      co_await co_spawn(
	 ex,
	 filler(reqs, st),
	 net::use_awaitable);

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

