/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/co_connection.hpp>
#include <boost/redis/config.hpp>

#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/corosio/io_context.hpp>

#include <exception>
#include <iostream>

namespace capy = boost::capy;
using namespace boost::redis;
namespace corosio = boost::corosio;

capy::io_task<> run_request(co_connection& conn)
{
   // A request containing only a ping command.
   request req;
   req.push("PING", "Hello world");

   // Response where the PONG response will be stored.
   response<std::string> resp;

   // Executes the request.
   auto [ec] = co_await conn.exec(req, resp);
   if (ec) {
      std::cout << "Error executing PING: " << ec << std::endl;
   } else {
      std::cout << "PING value: " << std::get<0>(resp).value() << std::endl;
   }

   co_return {};
}

capy::task<void> co_main()
{
   // Create a connection
   co_connection conn{co_await capy::this_coro::executor};

   // Run the connection and the PING request, in parallel
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
            exit(1);
         }
         exit(1);
      })(co_main());

   // Executes all pending work, including the main coroutine
   ctx.run();
}
