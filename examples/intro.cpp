/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <iostream>

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <aedis.hpp>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using endpoints = net::ip::tcp::resolver::results_type;

using aedis::adapt;
using aedis::resp3::request;
using connection = net::use_awaitable_t<>::as_default_on_t<aedis::connection>;

net::awaitable<void> ping(endpoints const& addrs)
{
   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3);
   req.push("PING");
   req.push("QUIT");

   std::tuple<aedis::ignore, std::string, aedis::ignore> resp;

   connection conn{co_await net::this_coro::executor};
   co_await net::async_connect(conn.next_layer(), addrs);
   co_await (conn.async_run() || conn.async_exec(req, adapt(resp)));

   std::cout << "PING: " << std::get<1>(resp) << std::endl;
}

auto main() -> int
{
   try {
      net::io_context ioc;
      net::ip::tcp::resolver resv{ioc};
      auto const addrs = resv.resolve("127.0.0.1", "6379");
      net::co_spawn(ioc, ping(addrs), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
   }
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)
auto main() -> int {std::cout << "Requires coroutine support." << std::endl; return 0;}
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
