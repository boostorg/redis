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

// TODO: re-add this to CMake!

namespace capy = boost::capy;
using namespace boost::redis;
namespace corosio = boost::corosio;

capy::task<void> run_request(co_connection& conn)
{
   // A request containing only a ping command.
   request req;
   req.push("PING", "Hello world");

   // Response where the PONG response will be stored.
   response<std::string> resp;

   // Executes the request.
   auto [ec] = co_await conn.exec(req, resp);
   if (ec)
      co_return;
   std::cout << "PING value: " << std::get<0>(resp).value() << std::endl;
}

capy::task<void> co_main()
{
   // Create a connection
   co_connection conn{(co_await capy::this_coro::executor).context()};

   auto r = co_await capy::when_any(run_request(conn), conn.run(config{}));

   static_cast<void>(r);
}

struct handler {
   void operator()() { std::cout << "Done\n"; }
   void operator()(std::exception_ptr exc)
   {
      if (exc)
         std::rethrow_exception(exc);
   }
};

int main()
{
   corosio::io_context ctx;
   capy::run_async(ctx.get_executor())(co_main());
   ctx.run();
}
