/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/co_connection.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/push_parser.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/buffers/string_dynamic_buffer.hpp>
#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/read_until.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/corosio/io_context.hpp>
#include <boost/corosio/stream_file.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <unistd.h>

namespace capy = boost::capy;
namespace corosio = boost::corosio;
using namespace boost::redis;

// Chat over Redis pubsub. To test, run this program from multiple
// terminals and type messages to stdin. Press Ctrl+D to exit.

namespace {

// Receives server pushes.
capy::io_task<> receiver(co_connection& conn)
{
   // Set the receive response, so pushes are stored in resp
   generic_flat_response resp;
   conn.set_receive_response(resp);

   // Subscribe to the channel 'channel'. Using request::subscribe()
   // (instead of request::push()) makes the connection re-subscribe
   // to 'channel' whenever it re-connects to the server.
   request req;
   req.subscribe({"channel"});
   auto [sub_ec] = co_await conn.exec(req);
   if (sub_ec) {
      std::cerr << "Error subscribing: " << sub_ec << std::endl;
      co_return {};
   }

   // Loop to read Redis push messages. The loop terminates when receive() reports
   // an error (e.g. cancellation when the surrounding when_any cascade tears down
   // the run loop).
   while (true) {
      // Wait for pushes
      auto [ec] = co_await conn.receive();

      // Check for errors and cancellations
      if (ec) {
         std::cerr << "Error during receive: " << ec << std::endl;
         co_return {};
      }

      // This can happen if a SUBSCRIBE command errored (e.g. insufficient permissions)
      if (resp.has_error()) {
         std::cerr << "The receive response contains an error: " << resp.error().diagnostic
                   << std::endl;
         co_return {};
      }

      // The response must be consumed without suspending the
      // coroutine i.e. without the use of async operations.
      for (push_view elem : push_parser(resp.value())) {
         std::cout << "Received message from channel " << elem.channel << ": " << elem.payload
                   << "\n";
      }

      std::cout << std::endl;

      resp.value().clear();
   }
}

// Publishes stdin messages to a Redis channel.
// Returns when stdin reaches EOF (e.g. Ctrl+D), which then cancels the surrounding
// when_any siblings.
capy::io_task<> publisher(corosio::stream_file& in, co_connection& conn)
{
   for (std::string msg;;) {
      auto [ec, n] = co_await capy::read_until(in, capy::string_dynamic_buffer(&msg), "\n");
      if (ec) {
         // EOF, cancellation or other read error: exit cleanly.
         std::cerr << "Error reading from stdin: " << ec << ": " << ec.message() << std::endl;
         co_return {};
      }

      request req;
      req.push("PUBLISH", "channel", msg);
      auto [pub_ec] = co_await conn.exec(req);
      if (pub_ec) {
         co_return {};
      }
      msg.erase(0, n);
   }
}

}  // namespace

capy::task<void> co_main(config cfg)
{
   auto ex = co_await capy::this_coro::executor;

   // Create a connection
   co_connection conn{ex};

   // Wrap a duplicated stdin file descriptor in a corosio stream
   corosio::stream_file in{ex};
   in.assign(::dup(STDIN_FILENO));

   // Run the connection, the receiver and the publisher in parallel.
   // The first task to complete (typically the publisher on EOF) cancels the others.
   co_await capy::when_any(receiver(conn), publisher(in, conn), conn.run(cfg));
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
      })(co_main(config{}));

   // Executes all pending work, including the main coroutine
   ctx.run();
}
