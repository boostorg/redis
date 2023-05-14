/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/detail/read.hpp>
#include <boost/redis/detail/write.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <string>
#include <iostream>
#define BOOST_TEST_MODULE conn-tls
#include <boost/test/included/unit_test.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace net = boost::asio;
namespace redis = boost::redis;
using resolver = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::resolver>;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using boost::redis::adapter::adapt2;
using net::ip::tcp;
using boost::redis::request;
using boost::redis::adapter::result;
using redis::config;

auto co_main(config cfg) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   resolver resv{ex};
   auto const addrs = co_await resv.async_resolve(cfg.addr.host, cfg.addr.port);
   tcp_socket socket{ex};
   co_await net::async_connect(socket, addrs);

   // Creates the request and writes to the socket.
   request req;
   req.push("HELLO", 3);
   req.push("PING", "Hello world");
   req.push("QUIT");
   co_await redis::detail::async_write(socket, req);

   // Responses
   std::string buffer;
   result<std::string> resp;

   // Reads the responses to all commands in the request.
   auto dbuffer = net::dynamic_buffer(buffer);
   co_await redis::detail::async_read(socket, dbuffer);
   co_await redis::detail::async_read(socket, dbuffer, adapt2(resp));
   co_await redis::detail::async_read(socket, dbuffer);

   std::cout << "Ping: " << resp.value() << std::endl;
}

BOOST_AUTO_TEST_CASE(low_level_async)
{
   net::io_context ioc;
   net::co_spawn(ioc, std::move(co_main({})), net::detached);
   ioc.run();
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)

BOOST_AUTO_TEST_CASE(low_level_async)
{
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
