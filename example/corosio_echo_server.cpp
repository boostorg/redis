/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/co_connection.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/buffers/make_buffer.hpp>
#include <boost/capy/buffers/string_dynamic_buffer.hpp>
#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/read_until.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/capy/write.hpp>
#include <boost/corosio/endpoint.hpp>
#include <boost/corosio/io_context.hpp>
#include <boost/corosio/signal_set.hpp>
#include <boost/corosio/tcp_acceptor.hpp>
#include <boost/corosio/tcp_socket.hpp>

#include <csignal>
#include <exception>
#include <iostream>
#include <string>
#include <utility>

namespace capy = boost::capy;
namespace corosio = boost::corosio;
using namespace boost::redis;

// Echoes lines received over TCP back to the sender, going through Redis (PING).
capy::io_task<> echo_server_session(corosio::tcp_socket socket, co_connection& conn)
{
   request req;
   response<std::string> resp;

   for (std::string buffer;;) {
      auto [read_ec, n] = co_await capy::read_until(
         socket,
         capy::string_dynamic_buffer(&buffer),
         "\n");
      if (read_ec) {
         std::cerr << "Error reading from session socket: " << read_ec << std::endl;
         co_return {};
      }

      req.push("PING", buffer);
      auto [exec_ec] = co_await conn.exec(req, resp);
      if (exec_ec) {
         std::cerr << "Error executing PING: " << exec_ec << std::endl;
         co_return {};
      }

      auto const& reply = std::get<0>(resp).value();
      auto [write_ec, written] = co_await capy::write(
         socket,
         capy::make_buffer(reply.data(), reply.size()));
      if (write_ec) {
         std::cerr << "Error writing to session socket: " << write_ec << std::endl;
         co_return {};
      }

      std::get<0>(resp).value().clear();
      req.clear();
      buffer.erase(0, n);
   }
}

// Listens for tcp connections.
capy::io_task<> listener(co_connection& conn)
{
   auto ex = co_await capy::this_coro::executor;
   corosio::tcp_acceptor acc{ex, corosio::endpoint(static_cast<std::uint16_t>(55555))};

   for (;;) {
      corosio::tcp_socket peer{ex};
      auto [ec] = co_await acc.accept(peer);
      if (ec) {
         std::clog << "Listener: " << ec.message() << std::endl;
         co_return {};
      }

      // Spawn the session as a detached task on the same executor.
      // The new task runs independently and does not inherit the listener's stop token.
      capy::run_async(ex)(echo_server_session(std::move(peer), conn));
   }
}

// Wait for SIGINT or SIGTERM; completing this task triggers a graceful shutdown
// by cancelling the surviving siblings via the when_any cascade.
capy::io_task<> signal_handler()
{
   corosio::signal_set signals{(co_await capy::this_coro::executor).context(), SIGINT, SIGTERM};
   auto [ec, signum] = co_await signals.wait();
   if (!ec)
      std::cout << "Received signal " << signum << ", shutting down\n";
   co_return {};
};

capy::task<void> co_main()
{
   // Create a connection
   co_connection conn{co_await capy::this_coro::executor};

   // Run the connection, the listener and the signal waiter in parallel.
   // when_any will cancel the surviving tasks once any one of them completes.
   co_await capy::when_any(listener(conn), conn.run(config{}), signal_handler());
}

int main()
{
   // The I/O context, required for all I/O operations
   corosio::io_context ctx;

   // Schedules the main coroutine for execution
   capy::run_async(
      ctx.get_executor(),
      []() {
         // Runs when the main coroutine finishes normally
         std::cout << "Done\n";
      },
      [](std::exception_ptr exc) {
         // Runs when the main coroutine finishes with an exception
         try {
            std::rethrow_exception(exc);
         } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            exit(1);
         }
         exit(1);
      })(co_main());

   // Executes all pending work, including the main coroutine
   ctx.run();
}
