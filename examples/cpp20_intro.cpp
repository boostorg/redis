/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/redis.hpp>
#include "common/common.hpp"

namespace net = boost::asio;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore_t;

auto run(std::shared_ptr<connection> conn, std::string host, std::string port) -> net::awaitable<void>
{
   // From examples/common.hpp to avoid vebosity
   co_await connect(conn, host, port);

   // async_run coordinate read and write operations.
   co_await conn->async_run();

   // Cancel pending operations, if any.
   conn->cancel(operation::exec);
   conn->cancel(operation::receive);
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
