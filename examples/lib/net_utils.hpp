/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/config.hpp>

using tcp_socket = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::ip::tcp::socket>;
using tcp_resolver = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::ip::tcp::resolver>;
using timer = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::steady_timer>;

aedis::net::awaitable<tcp_socket>
connect(
   std::string host = "127.0.0.1",
   std::string port = "6379")
{
   auto ex = co_await aedis::net::this_coro::executor;
   tcp_resolver resolver{ex};
   auto const res = co_await resolver.async_resolve(host, port);
   tcp_socket socket{ex};
   co_await aedis::net::async_connect(socket, res);
   co_return std::move(socket);
}

//net::awaitable<void>
//client::connection_manager()
//{
//   using namespace aedis::net::experimental::awaitable_operators;
//   using tcp_resolver = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::ip::tcp::resolver>;
//
//   for (;;) {
//      tcp_resolver resolver{socket_.get_executor()};
//      auto const res = co_await resolver.async_resolve("127.0.0.1", "6379");
//      co_await net::async_connect(socket_, res);
//
//      co_await say_hello();
//
//      timer_.expires_at(std::chrono::steady_clock::time_point::max());
//      co_await (reader() && writer());
//
//      socket_.close();
//      timer_.cancel();
//
//      timer_.expires_after(std::chrono::seconds{1});
//      boost::system::error_code ec;
//      co_await timer_.async_wait(net::redirect_error(net::use_awaitable, ec));
//   }
//}

