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
using aedis::redis::command;
using aedis::redis::experimental::client;
using aedis::user_session;
using aedis::user_session_base;
using aedis::resp3::node;
using aedis::resp3::adapt;
using aedis::resp3::type;

class receiver : public std::enable_shared_from_this<receiver> {
private:
   std::vector<node> resps_;
   std::queue<std::weak_ptr<user_session_base>> sessions_;

public:
   void on_message(command cmd)
   {
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
         default: { /* Ignore */; }
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

net::awaitable<void> run(std::shared_ptr<client> db, std::shared_ptr<receiver> recv)
{
   // Causes a bug.
   //db->set_stream(co_await connect());
   for (auto adapter = recv->get_adapter();;) {
     boost::system::error_code ec;
      auto const cmd = co_await db->async_read(adapter, net::redirect_error(net::use_awaitable, ec));
      if (ec)
        co_return;
      recv->on_message(cmd);
   }
}

net::awaitable<void>
connection_manager(std::shared_ptr<client> db, std::shared_ptr<receiver> recv)
{
   db->set_stream(co_await connect());
   db->send(command::hello, 3);
   co_await run(db, recv);
}

net::awaitable<void>
signal_handler(
   std::shared_ptr<net::ip::tcp::acceptor> acc,
   std::shared_ptr<client> db)
{
   auto ex = co_await net::this_coro::executor;

   net::signal_set signals(ex, SIGINT, SIGTERM);
   co_await signals.async_wait(net::use_awaitable);

   db->send(command::quit); // Closes the connection with redis.
   acc->cancel(); // Stop listening for new connections.
}

net::awaitable<void> listener()
{
   auto ex = co_await net::this_coro::executor;

   auto endpoint = net::ip::tcp::endpoint{net::ip::tcp::v4(), 55555};
   auto acc = std::make_shared<net::ip::tcp::acceptor>(ex, endpoint);
   auto db = std::make_shared<client>(ex);
   auto recv = std::make_shared<receiver>();

   net::co_spawn(ex, signal_handler(acc, db), net::detached);
   net::co_spawn(ex, connection_manager(db, recv), net::detached);

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
      net::io_context ioc{1};
      co_spawn(ioc, listener(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
