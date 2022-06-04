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
namespace generic = aedis::generic;
namespace adapter = aedis::adapter;
using aedis::resp3::node;
using aedis::redis::command;
using aedis::generic::request;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
using tcp_acceptor = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::acceptor>;
using connection = aedis::generic::connection<command, tcp_socket>;
using response_type = std::vector<aedis::resp3::node<std::string>>;

class user_session:
   public std::enable_shared_from_this<user_session> {
public:
   user_session(tcp_socket socket)
   : socket_(std::move(socket))
   , timer_(socket_.get_executor())
      { timer_.expires_at(std::chrono::steady_clock::time_point::max()); }

   void start(std::shared_ptr<connection> db)
   {
      co_spawn(socket_.get_executor(),
          [self = shared_from_this(), db]{ return self->reader(db); },
          net::detached);

      co_spawn(socket_.get_executor(),
          [self = shared_from_this()]{ return self->writer(); },
          net::detached);
   }

   void deliver(std::string const& msg)
   {
      write_msgs_.push_back(msg);
      timer_.cancel_one();
   }

private:
   net::awaitable<void> reader(std::shared_ptr<connection> db)
   {
      try {
         std::string msg;
         request<command> req;
         auto dbuffer = net::dynamic_buffer(msg, 1024);
         for (;;) {
            auto const n = co_await net::async_read_until(socket_, dbuffer, "\n");
            req.push(command::publish, "channel", msg);
            co_await db->async_exec(req, generic::adapt());
            req.clear();
            msg.erase(0, n);
         }
      } catch (std::exception&) {
         stop();
      }
   }

   net::awaitable<void> writer()
   {
      try {
         while (socket_.is_open()) {
            if (write_msgs_.empty()) {
               boost::system::error_code ec;
               co_await timer_.async_wait(net::redirect_error(net::use_awaitable, ec));
            } else {
               co_await net::async_write(socket_, net::buffer(write_msgs_.front()));
               write_msgs_.pop_front();
            }
         }
      } catch (std::exception&) {
        stop();
      }
   }

   void stop()
   {
      socket_.close();
      timer_.cancel();
   }

   tcp_socket socket_;
   net::steady_timer timer_;
   std::deque<std::string> write_msgs_;
};

using sessions_type = std::vector<std::shared_ptr<user_session>>;

net::awaitable<void>
reader(
   std::shared_ptr<connection> db,
   std::shared_ptr<sessions_type> sessions)
{
   request<command> req;
   req.push(command::subscribe, "channel");
   co_await db->async_exec(req, generic::adapt());

   for (response_type resp;;) {
      co_await db->async_read_push(adapter::adapt(resp));

      for (auto& session: *sessions)
         session->deliver(resp.at(3).value);

      resp.clear();
   }
}

net::awaitable<void>
listener(
    std::shared_ptr<tcp_acceptor> acc,
    std::shared_ptr<connection> db,
    std::shared_ptr<sessions_type> sessions)
{
   for (;;) {
      auto socket = co_await acc->async_accept();
      auto session = std::make_shared<user_session>(std::move(socket));
      sessions->push_back(session);
      session->start(db);
   }
}

auto handler =[](auto ec, auto...)
   { std::cout << ec.message() << std::endl; };

int main()
{
   try {
      net::io_context ioc{1};

      // Redis client and receiver.
      auto db = std::make_shared<connection>(ioc);
      db->async_run(handler);

      auto sessions = std::make_shared<sessions_type>();
      net::co_spawn(ioc, reader(db, sessions), net::detached);

      // TCP acceptor.
      auto endpoint = net::ip::tcp::endpoint{net::ip::tcp::v4(), 55555};
      auto acc = std::make_shared<tcp_acceptor>(ioc, endpoint);
      co_spawn(ioc, listener(acc, db, sessions), net::detached);

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
