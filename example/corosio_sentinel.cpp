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
   // You can use the connection normally, as you would use a connection to a single master.
   request req;
   req.push("PING", "Hello world");
   response<std::string> resp;

   // Execute the request.
   auto [ec] = co_await conn.exec(req, resp);
   if (ec) {
      std::cerr << "Error executing PING: " << ec << std::endl;
      exit(1);
   }

   std::cout << "PING: " << std::get<0>(resp).value() << std::endl;
   co_return {};
}

capy::task<void> co_main()
{
   // Boost.Redis has built-in support for Sentinel deployments.
   // To enable it, set the fields in config shown here.
   // sentinel.addresses should contain a list of (hostname, port) pairs
   // where Sentinels are listening. IPs can also be used.
   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "26379"},
      {"localhost", "26380"},
      {"localhost", "26381"},
   };

   // Set master_name to the identifier that you configured
   // in the "sentinel monitor" statement of your sentinel.conf file
   cfg.sentinel.master_name = "mymaster";

   // run() will contact the Sentinels, obtain the master address,
   // connect to it and keep the connection healthy. If a failover happens,
   // the address will be resolved again and the new elected master will be contacted.
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
            exit(1);
         }
         exit(1);
      })(co_main());

   // Executes all pending work, including the main coroutine
   ctx.run();
}
