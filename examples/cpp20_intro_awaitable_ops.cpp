/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/redis.hpp>
#include "common/common.hpp"

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore_t;

// Called from the main function (see main.cpp)
auto co_main(std::string host, std::string port) -> net::awaitable<void>
{
   try {
      request req;
      req.push("HELLO", 3);
      req.push("PING", "Hello world");
      req.push("QUIT");

      response<ignore_t, std::string, ignore_t> resp;

      auto conn = std::make_shared<connection>(co_await net::this_coro::executor);
      co_await connect(conn, host, port);
      co_await (conn->async_run() || conn->async_exec(req, resp));

      std::cout << "PING: " << std::get<1>(resp).value() << std::endl;
   } catch (std::exception const& e) {
      std::cout << e.what() << std::endl;
   }
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
