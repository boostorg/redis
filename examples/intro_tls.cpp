/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <iostream>

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ssl.hpp>

#include <aedis.hpp>
#include <aedis/ssl/connection.hpp>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using resolver = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::resolver>;

using aedis::adapt;
using aedis::resp3::request;
using connection = net::use_awaitable_t<>::as_default_on_t<aedis::ssl::connection>;

auto verify_certificate(bool, net::ssl::verify_context&) -> bool
{
   std::cout << "set_verify_callback" << std::endl;
   return true;
}

// Pass resolved address as paramter.
net::awaitable<void> ping()
{
   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3, "AUTH", "aedis", "aedis");
   req.push("PING");
   req.push("QUIT");

   std::tuple<aedis::ignore, std::string, aedis::ignore> resp;

   // Resolve
   auto ex = co_await net::this_coro::executor;
   resolver resv{ex};
   auto const endpoints = co_await resv.async_resolve("db.occase.de", "6380");

   net::ssl::context ctx{net::ssl::context::sslv23};
   connection conn{ex, ctx};
   conn.next_layer().set_verify_mode(net::ssl::verify_peer);
   conn.next_layer().set_verify_callback(verify_certificate);

   //auto f = [](boost::system::error_code const&, auto const&) { return true; };
   co_await net::async_connect(conn.lowest_layer(), endpoints);
   co_await conn.next_layer().async_handshake(net::ssl::stream_base::client);
   co_await (conn.async_run() || conn.async_exec(req, adapt(resp)));

   std::cout << "Response: " << std::get<1>(resp) << std::endl;
}

auto main() -> int
{
   try {
      net::io_context ioc;
      net::co_spawn(ioc, ping(), net::detached);
      ioc.run();
   } catch (...) {
      std::cerr << "Error." << std::endl;
   }
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)
auto main() -> int {std::cout << "Requires coroutine support." << std::endl; return 0;}
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
