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
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "user_session.hpp"

namespace net = boost::asio;
using aedis::resp3::node;
using aedis::adapter::adapt;
using aedis::generic::make_client_adapter;
using aedis::redis::command;
using aedis::user_session;
using aedis::user_session_base;
using client_type = aedis::generic::client<net::ip::tcp::socket, command>;
using response_type = std::vector<aedis::resp3::node<std::string>>;
using node_type = aedis::resp3::node<boost::string_view>;
using error_code = boost::system::error_code;
using net::experimental::as_tuple;
using namespace net::experimental::awaitable_operators;

class receiver {
public:
   net::awaitable<void> run(std::shared_ptr<client_type> db)
   {
      co_await (reconnect(db) && reader(db));
   }

   void add_user_session(std::shared_ptr<user_session_base> session)
      { sessions_.push(session); }

   void disable_reconnect() {reconnect_ = false;}

private:
   net::awaitable<void> reader(std::shared_ptr<client_type> db)
   {
      response_type resp;
      db->set_adapter(make_client_adapter<command>(adapt(resp)));

      for (;;) {
         auto [ec, cmd, n] = co_await db->async_read_one(as_tuple(net::use_awaitable));
         if (ec)
            co_return;

         switch (cmd) {
            case command::get:
            sessions_.front()->deliver(resp.front().value);
            sessions_.pop();
            break;

            case command::incr:
            std::cout << "Echos so far: " << resp.front().value << std::endl;
            break;

            default:;
         }

         resp.clear();
      }
   }

   net::awaitable<void> reconnect(std::shared_ptr<client_type> db)
   {
      auto ex = co_await net::this_coro::executor;
      boost::asio::steady_timer timer{ex};

      for (boost::system::error_code ec; reconnect_;) {
         co_await db->async_run(net::redirect_error(net::use_awaitable, ec));

         // Wait two seconds and try again.
         timer.expires_after(std::chrono::seconds{1});
         co_await timer.async_wait(net::redirect_error(net::use_awaitable, ec));
      }
   }

   bool reconnect_ = true;
   std::queue<std::shared_ptr<user_session_base>> sessions_;
};

net::awaitable<void>
listener(
    std::shared_ptr<net::ip::tcp::acceptor> acc,
    std::shared_ptr<client_type> db,
    std::shared_ptr<receiver> recv)
{
   for (;;) {
      auto socket = co_await acc->async_accept(net::use_awaitable);
      auto session = std::make_shared<user_session>(std::move(socket));

      auto on_user_msg = [db, recv, session](std::string const& msg)
      {
         db->send(command::set, "echo-server-key", msg);
         db->send(command::get, "echo-server-key");
         db->send(command::incr, "echo-server-counter");
         recv->add_user_session(session);
      };

      session->start(on_user_msg);
   }
}

int main()
{
   try {
      net::io_context ioc;

      // Redis client and receiver.
      auto db = std::make_shared<client_type>(ioc.get_executor());
      auto recv = std::make_shared<receiver>();
      co_spawn(ioc, [db, recv]{ return recv->run(db);}, net::detached);

      // TCP acceptor.
      auto endpoint = net::ip::tcp::endpoint{net::ip::tcp::v4(), 55555};
      auto acc = std::make_shared<net::ip::tcp::acceptor>(ioc.get_executor(), endpoint);
      co_spawn(ioc, listener(acc, db, recv), net::detached);

      // Signal handler.
      net::signal_set signals(ioc.get_executor(), SIGINT, SIGTERM);
      signals.async_wait([acc, db, recv] (auto, int) { 
            recv->disable_reconnect();
            acc->cancel();
            db->close();
      });

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
