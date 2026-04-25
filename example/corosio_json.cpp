/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/co_connection.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/response.hpp>

#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/corosio/io_context.hpp>
#include <boost/describe.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>

#include <exception>
#include <iostream>
#include <string>

namespace capy = boost::capy;
namespace corosio = boost::corosio;
namespace resp3 = boost::redis::resp3;
using namespace boost::describe;
using namespace boost::redis;
using boost::redis::resp3::node_view;

// Struct that will be stored in Redis using json serialization.
struct user {
   std::string name;
   std::string age;
   std::string country;
};

// The type must be described for serialization to work.
BOOST_DESCRIBE_STRUCT(user, (), (name, age, country))

// Boost.Redis customization points (example/json.hpp)
void boost_redis_to_bulk(std::string& to, user const& u)
{
   resp3::boost_redis_to_bulk(to, boost::json::serialize(boost::json::value_from(u)));
}

void boost_redis_from_bulk(user& u, node_view const& node, boost::system::error_code&)
{
   u = boost::json::value_to<user>(boost::json::parse(node.value));
}

capy::io_task<> run_request(co_connection& conn)
{
   // user object that will be stored in Redis in json format.
   user const u{"Joao", "58", "Brazil"};

   // Stores and retrieves in the same request.
   request req;
   req.push("SET", "json-key", u);  // Stores in Redis.
   req.push("GET", "json-key");     // Retrieves from Redis.

   response<ignore_t, user> resp;

   auto [ec] = co_await conn.exec(req, resp);
   if (ec) {
      std::cerr << "Error executing request: " << ec << std::endl;
      exit(1);
   }

   std::cout << "Name: " << std::get<1>(resp).value().name << "\n"
             << "Age: " << std::get<1>(resp).value().age << "\n"
             << "Country: " << std::get<1>(resp).value().country << "\n";

   co_return {};
}

capy::task<void> co_main()
{
   // Create a connection
   co_connection conn{co_await capy::this_coro::executor};

   // Run the connection and the JSON request in parallel.
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
