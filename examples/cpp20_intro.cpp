/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/run.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace net = boost::asio;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore_t;
using boost::redis::async_run;
using connection = boost::asio::use_awaitable_t<>::as_default_on_t<boost::redis::connection>;

auto run(std::shared_ptr<connection> conn, std::string host, std::string port) -> net::awaitable<void>
{
   // async_run coordinate read and write operations.
   co_await async_run(*conn, host, port);

   // Cancel pending operations, if any.
   conn->cancel();
}

// Called from the main function (see main.cpp)
auto co_main(std::string host, std::string port) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   net::co_spawn(ex, run(conn, host, port), net::detached);

   // A request can contain multiple commands.
   request req;
   req.push("HELLO", 3);
   req.push("PING", "Hello world");
   req.push("QUIT");

   // Stores responses of each individual command. The responses to
   // HELLO and QUIT are being ignored for simplicity.
   response<ignore_t, std::string, ignore_t> resp;

   // Executtes the request.
   co_await conn->async_exec(req, resp);

   std::cout << "PING: " << std::get<1>(resp).value() << std::endl;
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
