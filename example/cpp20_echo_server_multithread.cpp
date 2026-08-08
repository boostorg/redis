/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

/*
 * An echo server that answers every line it receives by PINGing a Redis server
 * with it, running on a multi-threaded execution context. A single connection
 * object is shared by all TCP sessions.
 *
 * Thread safety model
 * -------------------
 *
 * `basic_connection` follows the usual Asio I/O object convention: distinct
 * objects are safe to use concurrently, a single object is NOT. None of its
 * member functions provide any internal synchronization.
 *
 * To use a shared connection safely, you must use a strand. To invoke
 * a member function safely, your code must be running within the connection's strand.
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
using boost::redis::request;
using boost::redis::response;
using boost::redis::config;
using boost::system::error_code;
using boost::redis::connection;
using namespace std::chrono_literals;

// Handles a single TCP client. This coroutine is spawned on a strand of its
// own (see listener), so everything it owns is accessed by one thread at a time.
// All operations in a session are sequential, so we could leave this function
// unprotected. But using a strand here is future-proof: many operations, like
// asio::cancel_after, introduce parallelism, and the need for the strand.
auto echo_server_session(asio::ip::tcp::socket socket, std::shared_ptr<connection> conn)
   -> asio::awaitable<void>
{
   // These live in the coroutine frame, and are private to this session.
   request req;
   response<std::string> resp;
   std::string buffer;

   for (;;) {
      // All handlers scheduled by async_read_until run using the session's
      // strand because we're using C++20 coroutines.
      // Note that this is true even when the socket's executor is not a strand.
      auto n = co_await asio::async_read_until(socket, asio::dynamic_buffer(buffer, 1024), "\n");

      // async_read_until only guarantees that the delimiter is *somewhere* in
      // the buffer: a client that pipelines may have given us more than one
      // line. Take just the first one, and keep the rest for the next iteration.
      auto msg = std::string_view(buffer).substr(0u, n);
      req.push("PING", msg);

      // async_exec is an initiating function that reads and mutates the
      // connection's internal state, so it must run on the
      // connection's strand (not the session's).
      // co_spawn allows creating a new coroutine
      // bound to `conn->get_executor()`, which returns
      // the connection's strand.
      //
      // asio::use_awaitable produces a lazy awaitable
      // that is not started until co_spawn is co_await'ed.
      // By default, co_spawn returns an object that can be
      // co_await'ed, like other async operations.
      //
      // req and resp are private to this session and used outside
      // the session's strand, but this is fine because the caller is
      // suspended, and hasn't spawned any other parallel tasks.
      //
      // Treat strands like mutexes: hold them for at least as possible.
      // This is especially true for the connection strand, because it is
      // shared between all sessions.
      co_await asio::co_spawn(
         conn->get_executor(),
         conn->async_exec(req, resp, asio::use_awaitable));

      // We're now back on the session's strand.
      // Write the message back to the TCP client.
      co_await asio::async_write(socket, asio::buffer(std::get<0>(resp).value()));
      std::get<0>(resp).value().clear();
      req.clear();
      buffer.erase(0, n);
   }
}

// Listens for tcp connections.
//
// This coroutine runs directly on the pool executor rather than on a strand.
// That is fine: it is the only user of `acc`, and it never has more than one
// operation in flight on it, so the acceptor is never accessed concurrently
// even though successive resumptions may happen on different pool threads.
auto listener(std::shared_ptr<connection> conn) -> asio::awaitable<void>
{
   try {
      auto ex = co_await asio::this_coro::executor;
      asio::ip::tcp::acceptor acc(ex, {asio::ip::tcp::v4(), 55555});
      for (;;) {
         // Every session gets a strand of its own, so sessions run genuinely in
         // parallel on the pool while each individual session stays internally
         // serialized. `conn` is copied into the session -- copying the
         // shared_ptr is thread safe; using the object it points to is what
         // requires the connection's strand.
         asio::co_spawn(
            asio::make_strand(ex),
            echo_server_session(co_await acc.async_accept(), conn),
            asio::detached);
      }
   } catch (std::exception const& e) {
      std::clog << "Listener: " << e.what() << std::endl;
   }
}

// Completes when the user asks the server to stop. It touches no shared state.
auto wait_for_signals() -> asio::awaitable<void>
{
   auto ex = co_await asio::this_coro::executor;
   asio::signal_set sig_set(ex, SIGINT, SIGTERM);
   co_await sig_set.async_wait();
}

// Drives the connection. Spawned on the connection's strand
// because async_run accesses the connection's internal state.
//
// asio::as_tuple is used instead of the throwing default
// because async_run always completes with an error when cancelled.
auto run_connection(config cfg, std::shared_ptr<connection> conn) -> asio::awaitable<void>
{
   auto [ec] = co_await conn->async_run(cfg, asio::as_tuple);
   std::clog << "Run finished: " << ec << ": " << ec.message() << std::endl;
}

// The main coroutine, spawned by main()
auto co_main(config cfg) -> asio::awaitable<void>
{
   auto ex = co_await asio::this_coro::executor;

   // Create a strand, to be used as the connection's executor.
   // The connection is shared from multiple, parallel sessions,
   // so it needs protection.
   auto conn_strand = asio::make_strand(ex);

   // Create a connection, guarded by the strand
   auto conn = std::make_shared<connection>(conn_strand);

   // Run the three top-level tasks in parallel. Cancel the others
   // once one of them finishes.
   co_await asio::experimental::make_parallel_group(
      // Runs the TCP server. It does not use the strand because most of it
      // does not need access to the connection.
      // Strands are like mutexes - acquire them only when necessary, for the
      // shortest period of time possible.
      asio::co_spawn(ex, listener(conn)),

      // Runs the connection. This calls connection::async_run,
      // so it needs exclusive access to the connection.
      // On cancellation, co_spawn dispatches the handler through
      // the strand, so this pattern is safe. Don't use plain use_awaitable.
      asio::co_spawn(conn_strand, run_connection(std::move(cfg), conn)),

      // Waits for a signal to arrive, for clean shutdown.
      // Does not access the connection, so does not need protection.
      asio::co_spawn(ex, wait_for_signals()))
      .async_wait(asio::experimental::wait_for_one(), asio::deferred);
}

int main(int argc, char* argv[])
{
   try {
      // Parse the command line arguments
      config cfg;

      if (argc == 3) {
         cfg.addr.host = argv[1];
         cfg.addr.port = argv[2];
      }

      // Creates a thread pool with 4 threads.
      // This is a multi-threaded execution context: coroutines spawned on ctx
      // may resume on any of them.
      asio::thread_pool ctx{4u};
      asio::co_spawn(ctx, co_main(cfg), [](std::exception_ptr p) {
         if (p)
            std::rethrow_exception(p);
      });

      // Returns once co_main and everything it left running have finished.
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
