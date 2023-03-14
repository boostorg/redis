/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/run.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <iostream>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore_t;
using boost::redis::async_run;
using connection = boost::asio::use_awaitable_t<>::as_default_on_t<boost::redis::connection>;

// Called from the main function (see main.cpp)
auto co_main(std::string host, std::string port) -> net::awaitable<void>
{
   try {
      request req;
      req.push("HELLO", 3);
      req.push("PING", "Hello world");
      req.push("QUIT");

      response<ignore_t, std::string, ignore_t> resp;

      connection conn{co_await net::this_coro::executor};
      co_await (async_run(conn, host, port) || conn.async_exec(req, resp));

      std::cout << "PING: " << std::get<1>(resp).value() << std::endl;
   } catch (std::exception const& e) {
      std::cout << e.what() << std::endl;
   }
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
