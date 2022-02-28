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
namespace redis = aedis::redis;
using redis::receiver_tuple;
using aedis::redis::command;
using aedis::redis::client;
using aedis::redis::adapt;
using aedis::resp3::node;
using aedis::resp3::type;
using aedis::user_session;
using aedis::user_session_base;

// From lib/net_utils.hpp
using aedis::signal_handler;
using client_type = redis::client<net::detached_t::as_default_on_t<aedis::net::ip::tcp::socket>>;
using response_type = std::vector<node<std::string>>;

class receiver : public receiver_tuple<response_type>, std::enable_shared_from_this<receiver> {
public:
private:
   std::shared_ptr<client_type> db_;
   std::vector<std::shared_ptr<user_session_base>> sessions_;

public:
   receiver(std::shared_ptr<client_type> db) : db_{db} {}

   void on_message(command cmd)
   {
      switch (cmd) {
         case command::hello:
         db_->send(command::subscribe, "channel");
         break;

         case command::incr:
         std::cout << "Message so far: " << get<response_type>().front().value << std::endl;
         break;

         case command::unknown: // Server push
         for (auto& session: sessions_)
            session->deliver(get<response_type>().at(3).value);
         break;

         default: { /* Ignore */ }
      }

      get<response_type>().clear();
   }

   auto add(std::shared_ptr<user_session_base> session)
      { sessions_.push_back(session); }
};

net::awaitable<void> listener()
{
   auto ex = co_await net::this_coro::executor;

   auto endpoint = net::ip::tcp::endpoint{net::ip::tcp::v4(), 55555};
   auto acc = std::make_shared<net::ip::tcp::acceptor>(ex, endpoint);
   auto db = std::make_shared<client_type>(ex);
   auto recv = std::make_shared<receiver>(db);
   db->async_run(*recv);

   net::co_spawn(ex, signal_handler(acc, db), net::detached);

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
