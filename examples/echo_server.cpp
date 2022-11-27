/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <aedis.hpp>
#include "common.hpp"

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using tcp_acceptor = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::acceptor>;
using signal_set_type = net::use_awaitable_t<>::as_default_on_t<net::signal_set>;
using aedis::adapt;
using aedis::resp3::request;

auto echo_server_session(tcp_socket socket, std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   request req;
   std::string resp;

   for (std::string buffer;;) {
      auto n = co_await net::async_read_until(socket, net::dynamic_buffer(buffer, 1024), "\n");
      req.push("PING", buffer);
      auto tmp = std::tie(resp);
      co_await conn->async_exec(req, adapt(tmp));
      co_await net::async_write(socket, net::buffer(resp));
      resp.clear();
      req.clear();
      buffer.erase(0, n);
   }
}

// Listens for tcp connections.
auto listener(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   tcp_acceptor acc(ex, {net::ip::tcp::v4(), 55555});
   for (;;)
      net::co_spawn(ex, echo_server_session(co_await acc.async_accept(), conn), net::detached);
}

// Called from the main function (see common.cpp)
auto async_main() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   signal_set_type sig{ex, SIGINT, SIGTERM};

   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3);

   co_await connect(conn, "127.0.0.1", "6379");
   co_await ((conn->async_run() || listener(conn) || healthy_checker(conn) ||
            sig.async_wait()) && conn->async_exec(req));
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
