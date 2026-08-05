/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/config.hpp>
#include <boost/redis/connection.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/cancellation_condition.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <iostream>
#include <memory>
#include <string_view>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace asio = boost::asio;
using boost::asio::signal_set;
using boost::redis::request;
using boost::redis::response;
using boost::redis::config;
using boost::system::error_code;
using boost::redis::connection;
using namespace std::chrono_literals;

auto echo_server_session(asio::ip::tcp::socket socket, std::shared_ptr<connection> conn)
   -> asio::awaitable<void>
{
   request req;
   response<std::string> resp;
   std::string buffer;

   for (;;) {
      auto n = co_await asio::async_read_until(socket, asio::dynamic_buffer(buffer, 1024), "\n");
      auto msg = std::string_view(buffer).substr(0u, n);
      req.push("PING", msg);

      co_await asio::co_spawn(
         conn->get_executor(),
         conn->async_exec(req, resp, asio::use_awaitable));

      co_await asio::async_write(socket, asio::buffer(std::get<0>(resp).value()));
      std::get<0>(resp).value().clear();
      req.clear();
      buffer.erase(0, n);
   }
}

// Listens for tcp connections.
auto listener(std::shared_ptr<connection> conn) -> asio::awaitable<void>
{
   try {
      auto ex = co_await asio::this_coro::executor;
      asio::ip::tcp::acceptor acc(ex, {asio::ip::tcp::v4(), 55555});
      for (;;) {
         asio::co_spawn(
            asio::make_strand(ex),
            echo_server_session(co_await acc.async_accept(), conn),
            asio::detached);
      }
   } catch (std::exception const& e) {
      std::clog << "Listener: " << e.what() << std::endl;
   }
}

auto wait_for_signals() -> asio::awaitable<void>
{
   auto ex = co_await asio::this_coro::executor;
   asio::signal_set sig_set(ex, SIGINT, SIGTERM);
   co_await sig_set.async_wait();
}

auto my_run(config cfg, std::shared_ptr<connection> conn) -> asio::awaitable<void>
{
   std::clog << "Run starting" << std::endl;
   auto [ec] = co_await conn->async_run(cfg, asio::as_tuple);
   std::clog << "Run finished: " << ec << ": " << ec.message() << std::endl;
}

auto co_main(config cfg) -> asio::awaitable<void>
{
   auto ex = co_await asio::this_coro::executor;
   auto conn = std::make_shared<connection>(asio::make_strand(ex));

   co_await asio::experimental::make_parallel_group(
      asio::co_spawn(ex, listener(conn)),
      asio::co_spawn(conn->get_executor(), wait_for_signals()),
      asio::co_spawn(conn->get_executor(), my_run(std::move(cfg), conn)))
      .async_wait(asio::experimental::wait_for_one(), asio::deferred);
}

int main(int argc, char* argv[])
{
   try {
      config cfg;

      if (argc == 3) {
         cfg.addr.host = argv[1];
         cfg.addr.port = argv[2];
      }

      asio::thread_pool ctx{4u};
      asio::co_spawn(ctx, co_main(cfg), [](std::exception_ptr p) {
         if (p)
            std::rethrow_exception(p);
      });
      ctx.join();

   } catch (std::exception const& e) {
      std::cerr << "(main) " << e.what() << std::endl;
      return 1;
   }
}

#else  // defined(BOOST_ASIO_HAS_CO_AWAIT)

int main()
{
   std::cout << "Requires coroutine support." << std::endl;
   return 0;
}

#endif  // defined(BOOST_ASIO_HAS_CO_AWAIT)
