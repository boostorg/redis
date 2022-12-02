/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <aedis.hpp>
#include "common/common.hpp"

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using aedis::adapt;
using aedis::resp3::request;

// Called from the main function (see main.cpp)
auto async_main() -> net::awaitable<void>
{
   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3);
   req.push("PING", "Hello world");
   req.push("QUIT");

   std::tuple<aedis::ignore, std::string, aedis::ignore> resp;

   auto conn = std::make_shared<connection>(co_await net::this_coro::executor);
   co_await connect(conn, "127.0.0.1", "6379");
   co_await (conn->async_run() || conn->async_exec(req, adapt(resp)));

   std::cout << "PING: " << std::get<1>(resp) << std::endl;
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
