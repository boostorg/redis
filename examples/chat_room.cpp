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
using connection = aedis::generic::connection<command>;
using response_type = std::vector<aedis::resp3::node<std::string>>;

class user_session:
   public std::enable_shared_from_this<user_session> {
public:
   user_session(net::ip::tcp::socket socket)
   : socket_(std::move(socket))
   , timer_(socket_.get_executor())
      { timer_.expires_at(std::chrono::steady_clock::time_point::max()); }

   void
   start(std::shared_ptr<connection> db,
         std::shared_ptr<response_type> resp)
   {
      co_spawn(socket_.get_executor(),
          [self = shared_from_this(), db, resp]{ return self->reader(db, resp); },
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
   net::awaitable<void>
   reader(
      std::shared_ptr<connection> db,
      std::shared_ptr<response_type> resp)
   {
      try {
         for (std::string msg;;) {
            auto const n = co_await net::async_read_until(socket_, net::dynamic_buffer(msg, 1024), "\n", net::use_awaitable);
            request<command> req;
            req.push(command::publish, "channel", msg);
            req.push(command::incr, "chat-room-counter");
            co_await db->async_exec(req, generic::adapt(*resp), net::use_awaitable);
            std::cout << "Messsages so far: " << resp->at(1).value << std::endl;
            resp->clear();
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
               co_await net::async_write(socket_, net::buffer(write_msgs_.front()), net::use_awaitable);
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

   net::ip::tcp::socket socket_;
   net::steady_timer timer_;
   std::deque<std::string> write_msgs_;
};

using sessions_type = std::vector<std::shared_ptr<user_session>>;

net::awaitable<void>
reader(
   std::shared_ptr<connection> db,
   std::shared_ptr<response_type> resp,
   std::shared_ptr<sessions_type> sessions)
{
   for (;;) {
      co_await db->async_read_push(adapter::adapt(*resp), net::use_awaitable);

      for (auto& session: *sessions)
         session->deliver(resp->at(3).value);

      resp->clear();
   }
}

net::awaitable<void>
listener(
    std::shared_ptr<net::ip::tcp::acceptor> acc,
    std::shared_ptr<connection> db,
    std::shared_ptr<sessions_type> sessions,
    std::shared_ptr<response_type> resp)
{
   for (;;) {
      auto socket = co_await acc->async_accept(net::use_awaitable);
      auto session = std::make_shared<user_session>(std::move(socket));
      sessions->push_back(session);
      session->start(db, resp);
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

      // Sends hello and subscribes to the channel. Ignores the
      // response.
      request<command> req;
      req.push(command::subscribe, "channel");
      db->async_exec(req, generic::adapt(), handler);

      auto resp = std::make_shared<response_type>();

      auto sessions = std::make_shared<sessions_type>();
      net::co_spawn(ioc, reader(db, resp, sessions), net::detached);

      // TCP acceptor.
      auto endpoint = net::ip::tcp::endpoint{net::ip::tcp::v4(), 55555};
      auto acc = std::make_shared<net::ip::tcp::acceptor>(ioc.get_executor(), endpoint);
      co_spawn(ioc, listener(acc, db, sessions, resp), net::detached);

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
