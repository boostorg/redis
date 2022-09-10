/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <vector>
#include <iostream>
#include <tuple>

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <aedis.hpp>
#include "print.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using aedis::resp3::node;
using aedis::endpoint;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using stimer = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;
using connection = aedis::connection<tcp_socket>;

// Connects to a Redis instance over sentinel and performs failover in
// case of disconnection, see
// https://redis.io/docs/reference/sentinel-clients.  This example
// assumes a sentinel and a redis server running on localhost.

net::awaitable<void> push_receiver(std::shared_ptr<connection> db)
{
   for (std::vector<node<std::string>> resp;;) {
      co_await db->async_receive_push(adapt(resp));
      print_push(resp);
      resp.clear();
   }
}

net::awaitable<endpoint> resolve()
{
   // A list of sentinel addresses from which only one is responsive
   // to simulate sentinels that are down.
   std::vector<endpoint> const endpoints
   { {"foo", "26379"}
   , {"bar", "26379"}
   , {"127.0.0.1", "26379"}
   };

   request req1;
   req1.push("SENTINEL", "get-master-addr-by-name", "mymaster");
   req1.push("QUIT");

   auto ex = co_await net::this_coro::executor;
   connection conn{ex};

   std::tuple<std::optional<std::array<std::string, 2>>, aedis::ignore> addr;
   for (auto ep : endpoints) {
      boost::system::error_code ec;
      co_await conn.async_run(ep, req1, adapt(addr), net::redirect_error(net::use_awaitable, ec));
      conn.reset_stream();
      std::cout << ec.message() << std::endl;
      if (std::get<0>(addr))
         break;
   }

   endpoint ep;
   if (std::get<0>(addr)) {
      ep.host = std::get<0>(addr).value().at(0);
      ep.port = std::get<0>(addr).value().at(1);
   }

   co_return ep;
}

net::awaitable<void> reconnect(std::shared_ptr<connection> db)
{
   request req2;
   req2.push("SUBSCRIBE", "channel");

   auto ex = co_await net::this_coro::executor;
   stimer timer{ex};
   for (;;) {
      auto ep = co_await net::co_spawn(ex, resolve(), net::use_awaitable);
      if (!aedis::is_valid(ep)) {
         std::clog << "Can't resolve master name" << std::endl;
         co_return;
      }

      boost::system::error_code ec;
      co_await db->async_run(ep, req2, adapt(), net::redirect_error(net::use_awaitable, ec));
      std::cout << ec.message() << std::endl;
      std::cout << "Starting the failover." << std::endl;
      timer.expires_after(std::chrono::seconds{1});
      co_await timer.async_wait();
   }
}

int main()
{
   try {
      net::io_context ioc;
      auto db = std::make_shared<connection>(ioc);
      net::co_spawn(ioc, push_receiver(db), net::detached);
      net::co_spawn(ioc, reconnect(db), net::detached);
      net::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto){ ioc.stop(); });
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)
int main() {std::cout << "Requires coroutine support." << std::endl; return 1;}
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
