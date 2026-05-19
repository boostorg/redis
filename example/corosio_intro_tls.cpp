/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

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
#include <boost/corosio/tls_context.hpp>

#include <exception>
#include <iostream>
#include <utility>

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
      std::cout << "Error executing PING: " << ec << std::endl;
   } else {
      std::cout << "Response: " << std::get<0>(resp).value() << std::endl;
   }

   co_return {};
}

capy::task<void> co_main()
{
   // Configure a TLS connection to the public test server
   config cfg;
   cfg.use_ssl = true;
   cfg.username = "aedis";
   cfg.password = "aedis";
   cfg.addr.host = "db.occase.de";
   cfg.addr.port = "6380";

   // Configure the TLS context
   corosio::tls_context tls_ctx;
   if (auto ec = tls_ctx.set_verify_mode(corosio::tls_verify_mode::require_peer)) {
      std::cerr << "Error in set_verify_mode: " << ec << std::endl;
      exit(1);
   }
   tls_ctx.set_hostname("db.occase.de");

   // Create a connection using the configured TLS context
   co_connection conn{co_await capy::this_coro::executor, std::move(tls_ctx)};

   // Run the connection and the PING request, in parallel
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
