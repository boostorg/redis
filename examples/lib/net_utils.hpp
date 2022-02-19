/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/config.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

using tcp_socket = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::ip::tcp::socket>;
using tcp_resolver = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::ip::tcp::resolver>;
using timer = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::steady_timer>;

namespace aedis
{

aedis::net::awaitable<tcp_socket>
connect(
   std::string host = "127.0.0.1",
   std::string port = "6379")
{
   auto ex = co_await net::this_coro::executor;
   tcp_resolver resolver{ex};
   auto const res = co_await resolver.async_resolve(host, port);
   tcp_socket socket{ex};
   co_await aedis::net::async_connect(socket, res);
   co_return std::move(socket);
}

template <class Socket, class Receiver>
net::awaitable<void> reader(
   std::shared_ptr<redis::experimental::client<Socket>> db,
   std::shared_ptr<Receiver> recv)
{
   db->send(redis::command::hello, 3);

   for (auto adapter = recv->get_adapter();;) {
      boost::system::error_code ec;
      auto const cmd = co_await db->async_read(adapter, net::redirect_error(net::use_awaitable, ec));
      if (ec) {
         db->stop_writer();
         co_return;
      }

      recv->on_message(cmd);
   }
}

template <class Socket>
aedis::net::awaitable<void>
signal_handler(
   std::shared_ptr<aedis::net::ip::tcp::acceptor> acc,
   std::shared_ptr<aedis::redis::experimental::client<Socket>> db)
{
   auto ex = co_await aedis::net::this_coro::executor;

   aedis::net::signal_set signals(ex, SIGINT, SIGTERM);

   boost::system::error_code ec;
   co_await signals.async_wait(net::redirect_error(net::use_awaitable, ec));

   // Closes the connection with redis.
   db->send(aedis::redis::command::quit);

   // Stop listening for new connections.
   acc->cancel();
}

template <class T, class Socket>
net::awaitable<void>
connection_manager(
  std::shared_ptr<redis::experimental::client<Socket>> db,
  net::awaitable<T> reader)
{
   using namespace net::experimental::awaitable_operators;

   auto ex = co_await net::this_coro::executor;

   tcp_resolver resolver{ex};
   auto const res = co_await resolver.async_resolve("localhost", "6379");

   co_await net::async_connect(db->next_layer(), std::cbegin(res), std::end(res));
   co_await (db->async_writer() || std::move(reader));
}

} // aedis
