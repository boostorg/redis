/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <queue>
#include <vector>
#include <string>

#include <boost/asio/signal_set.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "user_session.hpp"

namespace net = boost::asio;
using aedis::resp3::node;
using aedis::adapter::adapt;
using aedis::adapter::adapter_t;
using aedis::redis::command;
using aedis::generic::client;
using aedis::user_session;
using aedis::user_session_base;
using client_type = client<net::ip::tcp::socket, command>;
using response_type = std::vector<node<std::string>>;

class receiver {
public:
   receiver(std::shared_ptr<client_type> db)
   : adapter_{adapt(resp_)}
   , db_{db}
   {}

   void on_resp3(command cmd, node<boost::string_view> const& nd, boost::system::error_code& ec)
   {
      adapter_(nd, ec);
   }

   void on_read(command cmd, std::size_t n)
   {
      switch (cmd) {
         case command::ping:
         if (resp_.front().value != "PONG") {
            sessions_.front()->deliver(resp_.front().value);
            sessions_.pop();
         }
         break;

         case command::incr:
         //std::cout << "Echos so far: " << resp_.front().value << std::endl;
         break;

         default: /* Ignore */;
      }

      resp_.clear();
   }

   void add_user_session(std::shared_ptr<user_session_base> session)
      { sessions_.push(session); }

private:
   response_type resp_;
   adapter_t<response_type> adapter_;
   std::shared_ptr<client_type> db_;
   std::queue<std::shared_ptr<user_session_base>> sessions_;
};

net::awaitable<void>
run_with_reconnect(std::shared_ptr<client_type> db)
{
   auto ex = co_await net::this_coro::executor;

   boost::asio::steady_timer timer{ex};

   for (boost::system::error_code ec;;) {
      co_await db->async_run(net::redirect_error(net::use_awaitable, ec));
      timer.expires_after(std::chrono::seconds{2});
      co_await timer.async_wait(net::redirect_error(net::use_awaitable, ec));
   }
}

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
         if (!msg.empty()) {
            db->send(command::ping, msg);
            db->send(command::incr, "echo-counter");
            recv->add_user_session(session);
         }
      };

      session->start(on_user_msg);
   }
}

int main()
{
   try {
      net::io_context ioc;

      auto db = std::make_shared<client_type>(ioc.get_executor());
      auto recv = std::make_shared<receiver>(db);
      db->set_receiver(recv);
      co_spawn(ioc, run_with_reconnect(db), net::detached);

      auto endpoint = net::ip::tcp::endpoint{net::ip::tcp::v4(), 55555};
      auto acc = std::make_shared<net::ip::tcp::acceptor>(ioc.get_executor(), endpoint);
      co_spawn(ioc, listener(acc, db, recv), net::detached);

      net::signal_set signals(ioc.get_executor(), SIGINT, SIGTERM);
      signals.async_wait([&] (auto, int) { ioc.stop(); });

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
