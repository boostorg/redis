/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <vector>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "lib/user_session.hpp"
#include "lib/net_utils.hpp"

namespace net = aedis::net;
using aedis::redis::command;
using aedis::redis::experimental::client;
using aedis::redis::experimental::adapt;
using aedis::resp3::node;
using aedis::resp3::type;
using aedis::user_session;
using aedis::user_session_base;

// From lib/net_utils.hpp
using aedis::connect;
using aedis::writer;
using aedis::reader;
using aedis::signal_handler;
using aedis::connection_manager;

// TODO: Delete sessions that have expired.
class receiver : public std::enable_shared_from_this<receiver> {
public:
private:
   std::shared_ptr<client> db_;
   std::vector<node> resps_;
   std::vector<std::shared_ptr<user_session_base>> sessions_;

public:
   receiver(std::shared_ptr<client> db) : db_{db} {}

   void on_message(command cmd)
   {
      switch (cmd) {
         case command::hello:
            db_->send(command::subscribe, "channel");
            break;

         case command::incr:
            std::cout << "Message so far: " << resps_.front().data << std::endl;
            break;

         case command::unknown: // Server push
            for (auto& session: sessions_)
               session->deliver(resps_.at(3).data);
            break;

         default: { /* Ignore */ }
      }

      resps_.clear();
   }

   auto get_adapter()
      { return aedis::redis::experimental::adapt(resps_); }

   auto add(std::shared_ptr<user_session_base> session)
      { sessions_.push_back(session); }
};

net::awaitable<void> listener()
{
   auto ex = co_await net::this_coro::executor;

   auto endpoint = net::ip::tcp::endpoint{net::ip::tcp::v4(), 55555};
   auto acc = std::make_shared<net::ip::tcp::acceptor>(ex, endpoint);
   auto db = std::make_shared<client>(ex);
   auto recv = std::make_shared<receiver>(db);

   net::co_spawn(ex, signal_handler(acc, db), net::detached);
   net::co_spawn(ex, connection_manager(db, recv), net::detached);

   auto on_user_msg = [db](std::string const& msg)
   {
      db->send(command::publish, "channel", msg);
      db->send(command::incr, "message-counter");
   };

   for (;;) {
      auto socket = co_await acc->async_accept(net::use_awaitable);
      auto session = std::make_shared<user_session>(std::move(socket));
      recv->add(session);
      session->start(on_user_msg);
   }
}

int main()
{
   try {
      net::io_context ioc{1};
      co_spawn(ioc, listener(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
