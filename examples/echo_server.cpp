/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <iostream>

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <aedis.hpp>
#include "reconnect.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using resolver = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::resolver>;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using tcp_acceptor = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::acceptor>;

using aedis::adapt;
using aedis::resp3::request;

auto echo_server_session(tcp_socket socket, std::shared_ptr<connection> db) -> net::awaitable<void>
{
   request req;
   std::tuple<std::string> response;

   for (std::string buffer;;) {
      auto n = co_await net::async_read_until(socket, net::dynamic_buffer(buffer, 1024), "\n");
      req.push("PING", buffer);
      co_await db->async_exec(req, adapt(response));
      co_await net::async_write(socket, net::buffer(std::get<0>(response)));
      std::get<0>(response).clear();
      req.clear();
      buffer.erase(0, n);
   }
}

auto listener(std::shared_ptr<connection> db) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   tcp_acceptor acc(ex, {net::ip::tcp::v4(), 55555});
   for (;;)
      net::co_spawn(ex, echo_server_session(co_await acc.async_accept(), db), net::detached);
}

auto main() -> int
{
   try {
      net::io_context ioc{1};
      auto db = std::make_shared<connection>(ioc);

      request req;
      req.get_config().cancel_on_connection_lost = true;
      req.push("HELLO", 3);

      co_spawn(ioc, reconnect(db, req, false), net::detached);
      co_spawn(ioc, listener(db), net::detached);

      net::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto) {
         ioc.stop();
      });

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)
auto main() -> int {std::cout << "Requires coroutine support." << std::endl; return 0;}
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
