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

net::awaitable<void> echo_loop(tcp_socket socket, std::shared_ptr<connection> db)
{
   try {
      for (std::string buffer;;) {
         auto n = co_await net::async_read_until(socket, net::dynamic_buffer(buffer, 1024), "\n");
         request<command> req;
         req.push(command::ping, buffer);
         std::tuple<std::string> resp;
         co_await db->async_exec(req, generic::adapt(resp));
         co_await net::async_write(socket, net::buffer(std::get<0>(resp)));
         buffer.erase(0, n);
      }
   } catch (std::exception const& e) {
      std::cout << e.what() << std::endl;
   }
}

net::awaitable<void> listener()
{
   auto ex = co_await net::this_coro::executor;
   auto db = std::make_shared<connection>(ex);
   db->async_run(net::detached);

   tcp_acceptor acc(ex, {net::ip::tcp::v4(), 55555});
   for (;;)
      net::co_spawn(ex, echo_loop(co_await acc.async_accept(), db), net::detached);
}

int main()
{
   try {
      net::io_context ioc;
      co_spawn(ioc, listener(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
