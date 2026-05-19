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

#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/corosio/io_context.hpp>

#include <exception>
#include <iostream>

namespace capy = boost::capy;
namespace corosio = boost::corosio;
using namespace boost::redis;

/* This example will subscribe and read pushes indefinitely.
 *
 * To test send messages with redis-cli
 *
 *    $ redis-cli -3
 *    127.0.0.1:6379> PUBLISH mychannel some-message
 *    (integer) 3
 *    127.0.0.1:6379>
 *
 * To test reconnection try, for example, to close all clients currently
 * connected to the Redis instance
 *
 * $ redis-cli
 * > CLIENT kill TYPE pubsub
 */

// Receives server pushes.
capy::io_task<> receiver(co_connection& conn)
{
   generic_flat_response resp;
   conn.set_receive_response(resp);

   // Subscribe to the channel 'mychannel'. You can add any number of channels here.
   request req;
   req.subscribe({"mychannel"});
   auto [sub_ec] = co_await conn.exec(req);
   if (sub_ec) {
      std::cerr << "Error subscribing: " << sub_ec << std::endl;
      co_return {};
   }

   // You're now subscribed to 'mychannel'. Pushes sent over this channel will be stored
   // in resp. If the connection encounters a network error and reconnects to the server,
   // it will automatically subscribe to 'mychannel' again. This is transparent to the user.
   // You need to use the specialized request::subscribe() function (instead of request::push)
   // to enable this behavior.

   // Loop to read Redis push messages. The loop terminates when receive() reports an error
   // (e.g. cancellation when the surrounding when_any cascade tears down the run loop).
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

      resp.value().clear();
   }
}

capy::task<void> co_main()
{
   // Create a connection
   co_connection conn{co_await capy::this_coro::executor};

   // Run the connection and the receiver loop in parallel.
   // when_any will cancel the surviving task once the other completes.
   co_await capy::when_any(receiver(conn), conn.run(config{}));
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
