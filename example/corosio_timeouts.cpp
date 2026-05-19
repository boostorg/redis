/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/co_connection.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/cond.hpp>
#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/timeout.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/corosio/io_context.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <string>

namespace capy = boost::capy;
namespace corosio = boost::corosio;
using namespace boost::redis;
using namespace std::chrono_literals;

capy::io_task<> run_request(co_connection& conn)
{
   // A request containing only a ping command.
   request req;
   req.push("PING", "Hello world");

   // Response where the PONG response will be stored.
   response<std::string> resp;

   // Executes the request with a timeout. If the server is down,
   // exec will wait until it's back again, so it may suspend for a long time.
   // For this reason, it's good practice to set a timeout to requests with capy::timeout.
   // If the request hasn't completed after 10 seconds, exec is cancelled
   // and the awaitable returns an error.
   auto [ec] = co_await capy::timeout(conn.exec(req, resp), 10s);
   if (ec) {
      std::cerr << "Error executing PING: " << ec << std::endl;
      exit(1);
   }

   std::cout << "PING: " << std::get<0>(resp).value() << std::endl;
   co_return {};
}

capy::task<void> co_main()
{
   // Create a connection
   co_connection conn{co_await capy::this_coro::executor};

   // Run the connection and the PING request in parallel.
   // when_any will cancel run() once the request completes.
   co_await capy::when_any(run_request(conn), conn.run(config{}));
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
         }
         exit(1);
      })(co_main());

   // Executes all pending work, including the main coroutine
   ctx.run();
}
