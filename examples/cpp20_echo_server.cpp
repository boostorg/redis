/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/co_spawn.hpp>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace net = boost::asio;
using tcp_socket = net::deferred_t::as_default_on_t<net::ip::tcp::socket>;
using tcp_acceptor = net::deferred_t::as_default_on_t<net::ip::tcp::acceptor>;
using signal_set = net::deferred_t::as_default_on_t<net::signal_set>;
using connection = net::deferred_t::as_default_on_t<boost::redis::connection>;
using boost::redis::request;
using boost::redis::response;
using boost::redis::config;
using boost::system::error_code;
using namespace std::chrono_literals;

auto echo_server_session(tcp_socket socket, std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   request req;
   response<std::string> resp;

   for (std::string buffer;;) {
      auto n = co_await net::async_read_until(socket, net::dynamic_buffer(buffer, 1024), "\n");
      req.push("PING", buffer);
      co_await conn->async_exec(req, resp);
      co_await net::async_write(socket, net::buffer(std::get<0>(resp).value()));
      std::get<0>(resp).value().clear();
      req.clear();
      buffer.erase(0, n);
   }
}

// Listens for tcp connections.
auto listener(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   try {
      auto ex = co_await net::this_coro::executor;
      tcp_acceptor acc(ex, {net::ip::tcp::v4(), 55555});
      for (;;)
         net::co_spawn(ex, echo_server_session(co_await acc.async_accept(), conn), net::detached);
   } catch (std::exception const& e) {
      std::clog << "Listener: " << e.what() << std::endl;
   }
}

// Called from the main function (see main.cpp)
auto co_main(config cfg) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   net::co_spawn(ex, listener(conn), net::detached);
   conn->async_run(cfg, {}, net::consign(net::detached, conn));

   signal_set sig_set(ex, SIGINT, SIGTERM);
   co_await sig_set.async_wait();
   conn->cancel();
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
