/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/system/errc.hpp>
#define BOOST_TEST_MODULE echo-stress
#include <boost/test/included/unit_test.hpp>
#include <boost/redis.hpp>
#include <boost/redis/src.hpp>
#include "common.hpp"
#include "../examples/common/common.hpp"

namespace net = boost::asio;
using error_code = boost::system::error_code;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;
using boost::redis::ignore_t;

auto push_consumer(std::shared_ptr<connection> conn, int expected) -> net::awaitable<void>
{
   int c = 0;
   for (;;) {
      co_await conn->async_receive(ignore, net::use_awaitable);
      if (++c == expected)
         break;
   }

   request req;
   req.push("HELLO", 3);
   req.push("QUIT");
   co_await conn->async_exec(req, ignore);
}

auto echo_session(std::shared_ptr<connection> conn, std::string id, int n) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   request req;
   response<ignore_t, std::string> resp;

   for (auto i = 0; i < n; ++i) {
      auto const msg = id + "/" + std::to_string(i);
      //std::cout << msg << std::endl;
      req.push("HELLO", 3);
      req.push("PING", msg);
      req.push("SUBSCRIBE", "channel");
      boost::system::error_code ec;
      co_await conn->async_exec(req, resp, redir(ec));
      BOOST_CHECK_EQUAL(ec, boost::system::error_code{});
      BOOST_CHECK_EQUAL(msg, std::get<1>(resp).value());
      req.clear();
      std::get<1>(resp).value().clear();
   }
}

auto async_echo_stress() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);

   int const sessions = 500;
   int const msgs = 1000;
   int total = sessions * msgs;

   net::co_spawn(ex, push_consumer(conn, total), net::detached);

   for (int i = 0; i < sessions; ++i) 
      net::co_spawn(ex, echo_session(conn, std::to_string(i), msgs), net::detached);

   co_await connect(conn, "127.0.0.1", "6379");
   co_await conn->async_run();
}

BOOST_AUTO_TEST_CASE(echo_stress)
{
   run(async_echo_stress());
}

#else
int main(){}
#endif
