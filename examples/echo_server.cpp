/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
namespace generic = aedis::generic;
using generic::request;
using aedis::redis::command;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using tcp_acceptor = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::acceptor>;
using connection = generic::connection<command, tcp_socket>;

net::awaitable<void>
echo_loop(tcp_socket socket, std::shared_ptr<connection> db)
{
   try {
      std::string msg;
      request<command> req;
      auto dbuffer = net::dynamic_buffer(msg, 1024);
      for (;;) {
         auto const n = co_await net::async_read_until(socket, dbuffer, "\n");
         req.push(command::incr, "echo-server-counter");
         req.push(command::ping, msg);
         std::tuple<std::string, std::string> resp;
         co_await db->async_exec(req, generic::adapt(resp));
         auto const msg = std::get<0>(resp) + ") " + std::get<1>(resp);
         co_await net::async_write(socket, net::buffer(msg));
         dbuffer.consume(n);
         req.clear();
      }
   } catch (std::exception const& e) {
      std::cout << "Error: " << e.what() << std::endl;
   }
}

net::awaitable<void>
listener(
    std::shared_ptr<tcp_acceptor> acc,
    std::shared_ptr<connection> db)
{
   auto ex = co_await net::this_coro::executor;

   for (;;) {
      auto socket = co_await acc->async_accept();
      net::co_spawn(ex, echo_loop(std::move(socket), db), net::detached);
   }
}

auto handler =[](auto ec, auto...)
   { std::cout << ec.message() << std::endl; };

int main()
{
   try {
      net::io_context ioc;

      // Redis client.
      auto db = std::make_shared<connection>(ioc);
      db->async_run(handler);

      // TCP acceptor.
      auto endpoint = net::ip::tcp::endpoint{net::ip::tcp::v4(), 55555};
      auto acc = std::make_shared<tcp_acceptor>(ioc, endpoint);
      co_spawn(ioc, listener(acc, db), net::detached);

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
