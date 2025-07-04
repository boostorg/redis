/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>

#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace net = boost::asio;
using boost::redis::config;
using boost::redis::generic_response;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::connection;
using net::signal_set;

auto stream_reader(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   std::string redisStreamKey_;
   request req;
   generic_response resp;

   std::string stream_id{"$"};
   std::string const field = "myfield";

   for (;;) {
      req.push("XREAD", "BLOCK", "0", "STREAMS", "test-topic", stream_id);
      co_await conn->async_exec(req, resp);

      //std::cout << "Response: ";
      //for (auto i = 0UL; i < resp->size(); ++i) {
      //    std::cout << resp->at(i).value << ", ";
      //}
      //std::cout << std::endl;

      // The following approach was taken in order to be able to
      // deal with the responses, as generated by redis in the case
      // that there are multiple stream 'records' within a single
      // generic_response.  The nesting and number of values in
      // resp.value() are different, depending on the contents
      // of the stream in redis.  Uncomment the above commented-out
      // code for examples while running the XADD command.

      std::size_t item_index = 0;
      while (item_index < std::size(resp.value())) {
         auto const& val = resp.value().at(item_index).value;

         if (field.compare(val) == 0) {
            // We've hit a myfield field.
            // The streamId is located at item_index - 2
            // The payload is located at item_index + 1
            stream_id = resp.value().at(item_index - 2).value;
            std::cout << "StreamId: " << stream_id << ", "
                      << "MyField: " << resp.value().at(item_index + 1).value << std::endl;
            ++item_index;  // We can increase so we don't read this again
         }

         ++item_index;
      }

      req.clear();
      resp.value().clear();
   }
}

// Run this in another terminal:
// redis-cli -r 100000 -i 0.0001 XADD "test-topic" "*" "myfield" "myfieldvalue1"
auto co_main(config cfg) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   net::co_spawn(ex, stream_reader(conn), net::detached);

   // Disable health checks.
   cfg.health_check_interval = std::chrono::seconds::zero();
   conn->async_run(cfg, net::consign(net::detached, conn));

   signal_set sig_set(ex, SIGINT, SIGTERM);
   co_await sig_set.async_wait();
   conn->cancel();
}
#endif  // defined(BOOST_ASIO_HAS_CO_AWAIT)
