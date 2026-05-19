//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/co_connection.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/corosio/io_context.hpp>

#include <exception>
#include <iostream>
#include <string>

namespace capy = boost::capy;
namespace corosio = boost::corosio;
using namespace boost::redis;

capy::io_task<> run_request(co_connection& conn)
{
   request req;
   req.push("PING");

   response<std::string> resp;

   auto [ec] = co_await conn.exec(req, resp);
   if (ec) {
      std::cerr << "Error executing PING: " << ec << std::endl;
      exit(1);
   }

   std::cout << "Response: " << std::get<0>(resp).value() << std::endl;
   co_return {};
}

capy::task<void> co_main()
{
   // If unix_socket is set to a non-empty string, UNIX domain sockets will be used
   // instead of TCP. Set this value to the path where your server is listening.
   // UNIX domain socket connections work in the same way as TCP connections.
   config cfg;
   cfg.unix_socket = "/tmp/redis-socks/redis.sock";

   // Create a connection
   co_connection conn{co_await capy::this_coro::executor};

   // Run the connection and the PING request in parallel.
   // when_any will cancel run() once the request completes.
   co_await capy::when_any(run_request(conn), conn.run(cfg));
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
