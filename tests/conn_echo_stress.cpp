/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/system/errc.hpp>

#define BOOST_TEST_MODULE low level
#include <boost/test/included/unit_test.hpp>

#include <aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;

using aedis::resp3::request;
using aedis::operation;
using aedis::adapt;
using connection = aedis::connection<>;
using endpoint = aedis::endpoint;
using error_code = boost::system::error_code;

#include <boost/asio/experimental/awaitable_operators.hpp>
using namespace net::experimental::awaitable_operators;

net::awaitable<void> push_consumer(std::shared_ptr<connection> conn, int expected)
{
   int c = 0;
   for (;;) {
      co_await conn->async_receive(adapt(), net::use_awaitable);
      if (++c == expected)
         break;
   }

   request req;
   req.push("QUIT");
   co_await conn->async_exec(req, adapt(), net::use_awaitable);
}

auto echo_session(std::shared_ptr<connection> conn, std::string id, int n) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   request req;
   std::tuple<std::string> resp;

   for (auto i = 0; i < n; ++i) {
      auto const msg = id + "/" + std::to_string(i);
      //std::cout << msg << std::endl;
      req.push("PING", msg);
      req.push("SUBSCRIBE", "channel");
      boost::system::error_code ec;
      co_await conn->async_exec(req, adapt(resp), net::redirect_error(net::use_awaitable, ec));
      BOOST_TEST(!ec);
      BOOST_CHECK_EQUAL(msg, std::get<0>(resp));
      req.clear();
      std::get<0>(resp).clear();
   }
}

auto async_echo_stress() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);

   int const sessions = 1000;
   int const msgs = 100;
   int total = sessions * msgs;

   net::co_spawn(ex, push_consumer(conn, total), net::detached);

   for (int i = 0; i < sessions; ++i) 
      net::co_spawn(ex, echo_session(conn, std::to_string(i), msgs), net::detached);

   endpoint ep{"127.0.0.1", "6379"};
   co_await conn->async_run(ep, {}, net::use_awaitable);
}

BOOST_AUTO_TEST_CASE(echo_stress)
{
   net::io_context ioc;
   net::co_spawn(ioc.get_executor(), async_echo_stress(), net::detached);
   ioc.run();
}

#else
int main(){}
#endif
