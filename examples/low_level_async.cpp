/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <iostream>

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)

#include <aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
namespace resp3 = aedis::resp3;
using endpoints = net::ip::tcp::resolver::results_type;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using aedis::resp3::request;
using aedis::adapter::adapt2;
using net::ip::tcp;

net::awaitable<void> ping(endpoints const& addrs)
{
   tcp_socket socket{co_await net::this_coro::executor};
   net::connect(socket, addrs);

   // Creates the request and writes to the socket.
   request req;
   req.push("HELLO", 3);
   req.push("PING");
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

int main()
{
   try {
      net::io_context ioc;
      net::ip::tcp::resolver resv{ioc};
      auto const addrs = resv.resolve("127.0.0.1", "6379");
      net::co_spawn(ioc, ping(addrs), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)
auto main() -> int {std::cout << "Requires coroutine support." << std::endl; return 0;}
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
