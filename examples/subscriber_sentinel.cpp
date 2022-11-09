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
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <aedis.hpp>
#include "print.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using aedis::adapt;
using aedis::resp3::request;
using aedis::resp3::node;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using stimer = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;
using connection = aedis::connection<tcp_socket>;

// Some shared example code.
#include "reconnect_sentinel.ipp"

// Connects to a Redis instance over sentinel and performs failover in
// case of disconnection, see
// https://redis.io/docs/reference/sentinel-clients.  This example
// assumes a sentinel and a redis server running on localhost.

net::awaitable<void> receiver(std::shared_ptr<connection> conn)
{
   for (std::vector<node<std::string>> resp;;) {
      co_await conn->async_receive(adapt(resp));
      print_push(resp);
      resp.clear();
   }
}

int main()
{
   try {
      net::io_context ioc;
      auto conn = std::make_shared<connection>(ioc);

      net::co_spawn(ioc, receiver(conn), net::detached);
      net::co_spawn(ioc, reconnect(conn), net::detached);

      net::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto){ ioc.stop(); });
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)
int main() {std::cout << "Requires coroutine support." << std::endl; return 0;}
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
