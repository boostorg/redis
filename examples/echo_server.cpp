/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <queue>
#include <vector>
#include <string>
#include <iostream>

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::resp3::node;
using aedis::adapter::adapt;
using aedis::generic::request;
using aedis::redis::command;
using client_type = aedis::generic::client<net::ip::tcp::socket, command>;
using response_type = std::vector<aedis::resp3::node<std::string>>;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using tcp_acceptor = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::acceptor>;

net::awaitable<void>
echo_loop(
   tcp_socket socket,
   std::shared_ptr<client_type> db,
   std::shared_ptr<response_type> resp)
{
   try {
      for (std::string msg;;) {
         auto const n = co_await net::async_read_until(socket, net::dynamic_buffer(msg, 1024), "\n", net::use_awaitable);
         request<command> req;
         req.push(command::set, "echo-server-key", msg);
         req.push(command::get, "echo-server-key");
         req.push(command::incr, "echo-server-counter");
         co_await db->async_exec(req, net::use_awaitable);
         co_await net::async_write(socket, net::buffer(resp->at(1).value), net::use_awaitable);
         std::cout << "Echos so far: " << resp->at(2) << std::endl;
         resp->clear();
         msg.erase(0, n);
      }
   } catch (std::exception const& e) {
      std::cout << "Error: " << e.what() << std::endl;
   }
}

net::awaitable<void>
listener(
    std::shared_ptr<tcp_acceptor> acc,
    std::shared_ptr<client_type> db,
    std::shared_ptr<response_type> resp)
{
   auto ex = co_await net::this_coro::executor;

   for (;;) {
      auto socket = co_await acc->async_accept();
      net::co_spawn(ex, echo_loop(std::move(socket), db, resp), net::detached);
   }
}

int main()
{
   try {
      net::io_context ioc;

      // Redis client.
      auto resp = std::make_shared<response_type>();
      auto db = std::make_shared<client_type>(ioc.get_executor(), adapt(*resp));
      db->async_run([](auto ec){ std::cout << ec.message() << std::endl;});

      // Sends hello and ignores the response.
      request<command> req;
      req.push(command::hello, 3);
      db->async_exec(req, [](auto, auto, auto){});

      // TCP acceptor.
      auto endpoint = net::ip::tcp::endpoint{net::ip::tcp::v4(), 55555};
      auto acc = std::make_shared<tcp_acceptor>(ioc.get_executor(), endpoint);
      co_spawn(ioc, listener(acc, db, resp), net::detached);

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
