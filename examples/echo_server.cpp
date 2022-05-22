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

class user_session: public std::enable_shared_from_this<user_session> {
public:
   user_session(net::ip::tcp::socket socket)
   : socket_(std::move(socket))
   { }

   void start(std::shared_ptr<client_type> db)
   {
      co_spawn(socket_.get_executor(),
          [self = shared_from_this(), db]{ return self->echo_loop(db); },
          net::detached);
   }

private:
   net::awaitable<void>
   echo_loop(std::shared_ptr<client_type> db)
   {
      std::vector<aedis::resp3::node<std::string>> resp;
      db->set_adapter(adapt(resp));

      try {
         for (std::string msg;;) {
            auto const n = co_await net::async_read_until(socket_, net::dynamic_buffer(msg, 1024), "\n", net::use_awaitable);
            request<command> req;
            req.push(command::set, "echo-server-key", msg);
            req.push(command::get, "echo-server-key");
            req.push(command::incr, "echo-server-counter");
            co_await db->async_exec(req, net::use_awaitable);
            co_await net::async_write(socket_, net::buffer(resp.at(1).value), net::use_awaitable);
            std::cout << "Echos so far: " << resp.at(2) << std::endl;
            resp.clear();
            msg.erase(0, n);
         }
      } catch (std::exception&) {
         socket_.close();
      }
   }

   net::ip::tcp::socket socket_;
};

net::awaitable<void>
listener(
    std::shared_ptr<net::ip::tcp::acceptor> acc,
    std::shared_ptr<client_type> db)
{
   for (;;) {
      auto socket = co_await acc->async_accept(net::use_awaitable);
      auto session = std::make_shared<user_session>(std::move(socket));
      session->start(db);
   }
}

int main()
{
   try {
      net::io_context ioc;

      // Redis client.
      auto db = std::make_shared<client_type>(ioc.get_executor());
      db->async_run([](auto ec){ std::cout << ec.message() << std::endl;});

      // Sends hello and ignores the response.
      request<command> req;
      req.push(command::hello, 3);
      db->async_exec(req, [](auto, auto, auto){});

      // TCP acceptor.
      auto endpoint = net::ip::tcp::endpoint{net::ip::tcp::v4(), 55555};
      auto acc = std::make_shared<net::ip::tcp::acceptor>(ioc.get_executor(), endpoint);
      co_spawn(ioc, listener(acc, db), net::detached);

      // Signal handler.
      net::signal_set signals(ioc.get_executor(), SIGINT, SIGTERM);
      signals.async_wait([acc, db] (auto, int) { 
            acc->cancel();
            db->close();
      });

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
