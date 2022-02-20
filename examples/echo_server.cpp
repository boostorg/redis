/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <queue>
#include <vector>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "lib/user_session.hpp"
#include "lib/net_utils.hpp"

namespace net = aedis::net;
namespace redis = aedis::redis;
using aedis::redis::command;
using aedis::redis::client;
using aedis::resp3::node;
using aedis::user_session;
using aedis::user_session_base;

// From lib/net_utils.hpp
using aedis::signal_handler;
using client_type = client<aedis::net::ip::tcp::socket>;

class receiver : public std::enable_shared_from_this<receiver> {
private:
   std::vector<node> resps_;
   std::queue<std::shared_ptr<user_session_base>> sessions_;

public:
   void on_message(command cmd)
   {
      switch (cmd) {
         case command::ping:
            sessions_.front()->deliver(resps_.front().data);
            sessions_.pop();
            break;

         case command::incr:
            std::cout << "Echos so far: " << resps_.front().data << std::endl;
            break;

         default:
            { /* Ignore */; }
      }

      resps_.clear();
   }

   auto adapter()
      { return redis::adapt(resps_); }

   void add_user_session(std::shared_ptr<user_session_base> session)
      { sessions_.push(session); }
};

net::awaitable<void> listener()
{
   auto ex = co_await net::this_coro::executor;

   auto endpoint = net::ip::tcp::endpoint{net::ip::tcp::v4(), 55555};
   auto acc = std::make_shared<net::ip::tcp::acceptor>(ex, endpoint);
   auto db = std::make_shared<client_type>(ex);

   auto recv = std::make_shared<receiver>();
   db->set_response_adapter(recv->adapter());
   db->set_reader_callback([recv](command cmd) {recv->on_message(cmd);});

   auto on_run = [](auto const ec)
      { std::clog << "Lost connection to redis: " << ec.message() << std::endl;};

   db->async_run("localhost", "6379", on_run);

   net::co_spawn(ex, signal_handler(acc, db), net::detached);

   for (;;) {
      auto socket = co_await acc->async_accept(net::use_awaitable);
      auto session = std::make_shared<user_session>(std::move(socket));

      auto on_user_msg = [db, recv, session](std::string const& msg)
      {
         db->send(command::ping, msg);
         db->send(command::incr, "echo-counter");
         recv->add_user_session(session);
      };

      session->start(on_user_msg);
   }
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
