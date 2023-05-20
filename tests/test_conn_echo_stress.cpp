/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/system/errc.hpp>
#define BOOST_TEST_MODULE echo-stress
#include <boost/test/included/unit_test.hpp>
#include <iostream>
#include "common.hpp"

#ifdef BOOST_ASIO_HAS_CO_AWAIT

namespace net = boost::asio;
using error_code = boost::system::error_code;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;
using boost::redis::ignore_t;
using boost::redis::logger;
using boost::redis::config;
using boost::redis::connection;

auto push_consumer(std::shared_ptr<connection> conn, int expected) -> net::awaitable<void>
{
   int c = 0;
   for (;;) {
      co_await conn->async_receive(ignore, net::use_awaitable);
      if (++c == expected)
         break;
   }

   conn->cancel();
}

auto
echo_session(
   std::shared_ptr<connection> conn,
   std::shared_ptr<request> pubs,
   std::string id,
   int n) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   request req;
   response<ignore_t, std::string, ignore_t> resp;

   for (auto i = 0; i < n; ++i) {
      auto const msg = id + "/" + std::to_string(i);
      //std::cout << msg << std::endl;
      req.push("HELLO", 3); // Just to mess around.
      req.push("PING", msg);
      req.push("PING", "lsls"); // TODO: Change to HELLO after fixing issue 105.
      boost::system::error_code ec;
      co_await conn->async_exec(req, resp, redir(ec));

      BOOST_REQUIRE_EQUAL(ec, boost::system::error_code{});
      BOOST_REQUIRE_EQUAL(msg, std::get<1>(resp).value());
      req.clear();
      std::get<1>(resp).value().clear();

      co_await conn->async_exec(*pubs, ignore, net::deferred);
   }
}

auto async_echo_stress() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   config cfg;
   cfg.health_check_interval = std::chrono::seconds::zero();
   run(conn, cfg,
       boost::asio::error::operation_aborted,
       boost::redis::operation::receive,
       boost::redis::logger::level::crit);

   request req;
   req.push("SUBSCRIBE", "channel");
   co_await conn->async_exec(req, ignore, net::deferred);

   // Number of coroutines that will send pings sharing the same
   // connection to redis.
   int const sessions = 500;

   // The number of pings that will be sent by each session.
   int const msgs = 1000;

   // The number of publishes that will be sent by each session with
   // each message.
   int const n_pubs = 10;

   // This is the total number of pushes we will receive.
   int total_pushes = sessions * msgs * n_pubs + 1;

   auto pubs = std::make_shared<request>();
   for (int i = 0; i < n_pubs; ++i)
      pubs->push("PUBLISH", "channel", "payload");

   // Op that will consume the pushes counting down until all expected
   // pushes have been received.
   net::co_spawn(ex, push_consumer(conn, total_pushes), net::detached);

   for (int i = 0; i < sessions; ++i) 
      net::co_spawn(ex, echo_session(conn, pubs, std::to_string(i), msgs), net::detached);
}

BOOST_AUTO_TEST_CASE(echo_stress)
{
   net::io_context ioc;
   net::co_spawn(ioc, async_echo_stress(), net::detached);
   ioc.run();
}

#else
BOOST_AUTO_TEST_CASE(dummy)
{
   BOOST_TEST(true);
}
#endif
