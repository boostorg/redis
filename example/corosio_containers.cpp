/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/co_connection.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/corosio/io_context.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace capy = boost::capy;
namespace corosio = boost::corosio;
using namespace boost::redis;

template <class T>
std::ostream& operator<<(std::ostream& os, std::optional<T> const& opt)
{
   if (opt.has_value())
      std::cout << opt.value();
   else
      std::cout << "null";

   return os;
}

void print(std::map<std::string, std::string> const& cont)
{
   for (auto const& e : cont)
      std::cout << e.first << ": " << e.second << "\n";
}

template <class T>
void print(std::vector<T> const& cont)
{
   for (auto const& e : cont)
      std::cout << e << " ";
   std::cout << "\n";
}

// Stores the content of some STL containers in Redis.
capy::task<> store(co_connection& conn)
{
   std::vector<int> vec{1, 2, 3, 4, 5, 6};

   std::map<std::string, std::string> map{
      {"key1", "value1"},
      {"key2", "value2"},
      {"key3", "value3"}
   };

   request req;
   req.push_range("RPUSH", "rpush-key", vec);
   req.push_range("HSET", "hset-key", map);
   req.push("SET", "key", "value");

   auto [ec] = co_await conn.exec(req, ignore);
   if (ec) {
      std::cerr << "Error in store: " << ec << std::endl;
      exit(1);
   }
}

capy::task<> hgetall(co_connection& conn)
{
   // A request contains multiple commands.
   request req;
   req.push("HGETALL", "hset-key");

   // Responses as tuple elements.
   response<std::map<std::string, std::string>> resp;

   // Executes the request and reads the response.
   auto [ec] = co_await conn.exec(req, resp);
   if (ec) {
      std::cerr << "Error in hgetall: " << ec << std::endl;
      exit(1);
   }

   print(std::get<0>(resp).value());
}

capy::task<> mget(co_connection& conn)
{
   // A request contains multiple commands.
   request req;
   req.push("MGET", "key", "non-existing-key");

   // Responses as tuple elements.
   response<std::vector<std::optional<std::string>>> resp;

   // Executes the request and reads the response.
   auto [ec] = co_await conn.exec(req, resp);
   if (ec) {
      std::cerr << "Error in mget: " << ec << std::endl;
      exit(1);
   }

   print(std::get<0>(resp).value());
}

// Retrieves in a transaction.
capy::task<> transaction(co_connection& conn)
{
   request req;
   req.push("MULTI");
   req.push("LRANGE", "rpush-key", 0, -1);  // Retrieves
   req.push("HGETALL", "hset-key");         // Retrieves
   req.push("MGET", "key", "non-existing-key");
   req.push("EXEC");

   response<
      ignore_t,  // multi
      ignore_t,  // lrange
      ignore_t,  // hgetall
      ignore_t,  // mget
      response<
         std::optional<std::vector<int>>,
         std::optional<std::map<std::string, std::string>>,
         std::optional<std::vector<std::optional<std::string>>>>  // exec
      >
      resp;

   auto [ec] = co_await conn.exec(req, resp);
   if (ec) {
      std::cerr << "Error in transaction: " << ec << std::endl;
      exit(1);
   }

   print(std::get<0>(std::get<4>(resp).value()).value().value());
   print(std::get<1>(std::get<4>(resp).value()).value().value());
   print(std::get<2>(std::get<4>(resp).value()).value().value());
}

capy::task<void> co_main(config cfg)
{
   // Create a connection
   co_connection conn{co_await capy::this_coro::executor};

   // Run the connection in parallel with the work coroutine.
   // when_any will cancel run() once the work completes.
   co_await capy::when_any(
      [&]() -> capy::io_task<> {
         co_await store(conn);
         co_await transaction(conn);
         co_await hgetall(conn);
         co_await mget(conn);
         co_return {};
      }(),
      conn.run(cfg));
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
