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

namespace net = aedis::net;
using aedis::redis::command;
using aedis::user_session;
using aedis::user_session_base;
using aedis::resp3::node;
using aedis::resp3::adapt;
using aedis::resp3::experimental::client;
using aedis::resp3::type;

class receiver : public std::enable_shared_from_this<receiver> {
private:
   std::vector<node> resps_;
   std::queue<std::weak_ptr<user_session_base>> sessions_;

public:
   void on_message(std::error_code ec, command cmd)
   {
      if (ec) {
         std::cerr << "Error: " << ec.message() << std::endl;
         return;
      }

      switch (cmd) {
         case command::ping:
         {
            if (auto session = sessions_.front().lock()) {
               session->deliver(resps_.front().data);
            } else {
               std::cout << "Session expired." << std::endl;
            }

            sessions_.pop();
         } break;
         case command::incr:
         {
            std::cout << "Echos so far: " << resps_.front().data << std::endl;
         } break;
         default: { assert(false); }
      }

      resps_.clear();
   }

   auto get_adapter()
   {
      return [adapter = adapt(resps_)](command, type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec) mutable
         { return adapter(t, aggregate_size, depth, data, size, ec); };
   }

   void add_user_session(std::shared_ptr<user_session_base> session)
      { sessions_.push(session); }
};

net::awaitable<void> listener()
{
   auto ex = co_await net::this_coro::executor;
   net::ip::tcp::acceptor acceptor(ex, {net::ip::tcp::v4(), 55555});

   auto recv = std::make_shared<receiver>();
   auto on_db_msg = [recv](std::error_code ec, command cmd)
      { recv->on_message(ec, cmd); };

   auto db = std::make_shared<client>(ex);
   db->set_adapter(recv->get_adapter());
   db->set_msg_callback(on_db_msg);
   db->prepare();

   for (;;) {
      auto socket = co_await acceptor.async_accept(net::use_awaitable);
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
      net::io_context ioc{1};
      net::signal_set signals(ioc, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto){ ioc.stop(); });
      co_spawn(ioc, listener(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
