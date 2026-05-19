//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/co_connection.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/corosio/io_context.hpp>

#include <exception>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>

namespace capy = boost::capy;
namespace corosio = boost::corosio;
namespace redis = boost::redis;

// Maps a Boost.Redis log level to a spdlog log level
static spdlog::level::level_enum to_spdlog_level(redis::logger::level lvl)
{
   switch (lvl) {
      // spdlog doesn't include the emerg and alert syslog levels,
      // so we convert them to the highest supported level.
      // Similarly, notice is similar to info
      case redis::logger::level::emerg:
      case redis::logger::level::alert:
      case redis::logger::level::crit:    return spdlog::level::critical;
      case redis::logger::level::err:     return spdlog::level::err;
      case redis::logger::level::warning: return spdlog::level::warn;
      case redis::logger::level::notice:
      case redis::logger::level::info:    return spdlog::level::info;
      case redis::logger::level::debug:
      default:                            return spdlog::level::debug;
   }
}

// This function glues Boost.Redis logging and spdlog.
// It should have the signature shown here. It will be invoked
// by Boost.Redis whenever a message is to be logged.
static void do_log(redis::logger::level level, std::string_view msg)
{
   spdlog::log(to_spdlog_level(level), "(Boost.Redis) {}", msg);
}

capy::io_task<> run_request(redis::co_connection& conn)
{
   // Execute a request
   redis::request req;
   req.push("PING", "Hello world");
   redis::response<std::string> resp;

   auto [ec] = co_await conn.exec(req, resp);
   if (ec) {
      spdlog::error("Request failed: {}", ec.message());
      exit(1);
   }

   spdlog::info("PING: {}", std::get<0>(resp).value());
   co_return {};
}

capy::task<void> co_main(redis::config cfg)
{
   // Create a connection to connect to Redis, and pass it a custom logger.
   // Boost.Redis will call do_log whenever it needs to log a message.
   // Note that the function will only be called for messages with level >= info
   // (i.e. filtering is done by Boost.Redis).
   redis::co_connection conn{
      co_await capy::this_coro::executor,
      redis::logger{redis::logger::level::info, do_log}
   };

   // Run the connection and the PING request in parallel.
   // when_any will cancel run() once the request completes.
   co_await capy::when_any(run_request(conn), conn.run(cfg));
}

int main(int argc, char** argv)
{
   try {
      // Configuration to connect to the server. Adjust as required
      redis::config cfg;
      if (argc == 3) {
         cfg.addr.host = argv[1];
         cfg.addr.port = argv[2];
      }

      // Create an execution context, required to create any I/O objects
      corosio::io_context ctx;

      // Schedules the main coroutine for execution
      capy::run_async(
         ctx.get_executor(),
         []() {
            // Runs when the main coroutine finishes normally
         },
         [](std::exception_ptr exc) {
            // Runs when the main coroutine finishes with an exception
            try {
               std::rethrow_exception(exc);
            } catch (const std::exception& e) {
               spdlog::error("Error: {}", e.what());
            }
            exit(1);
         })(co_main(cfg));

      // Actually run our example. Nothing will happen until we call run()
      ctx.run();

   } catch (std::exception const& e) {
      spdlog::error("Error: {}", e.what());
      return 1;
   }
}
