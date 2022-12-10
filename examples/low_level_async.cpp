/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <aedis.hpp>
#include <string>
#include <iostream>

namespace net = boost::asio;
namespace resp3 = aedis::resp3;
using resolver = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::resolver>;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using aedis::adapter::adapt2;
using net::ip::tcp;

auto async_main() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   resolver resv{ex};
   auto const addrs = co_await resv.async_resolve("127.0.0.1", "6379");
   tcp_socket socket{ex};
   co_await net::async_connect(socket, addrs);

   // Creates the request and writes to the socket.
   resp3::request req;
   req.push("HELLO", 3);
   req.push("PING", "Hello world");
   req.push("QUIT");
   co_await resp3::async_write(socket, req);

   // Responses
   std::string buffer, resp;

   // Reads the responses to all commands in the request.
   auto dbuffer = net::dynamic_buffer(buffer);
   co_await resp3::async_read(socket, dbuffer);
   co_await resp3::async_read(socket, dbuffer, adapt2(resp));
   co_await resp3::async_read(socket, dbuffer);

   std::cout << "Ping: " << resp << std::endl;
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
