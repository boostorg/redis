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
using aedis::resp3::adapt;
using aedis::resp3::node;
using aedis::resp3::type;
using aedis::user_session;
using aedis::user_session_base;

// TODO: Delete sessions that have expired.
class receiver : public std::enable_shared_from_this<receiver> {
public:
private:
   std::vector<node> resps_;
   std::vector<std::weak_ptr<user_session_base>> sessions_;

public:
   void on_message(command cmd)
   {
      switch (cmd) {
         case command::incr:
         {
            std::cout << "Message so far: " << resps_.front().data << std::endl;
         } break;
         case command::unknown: // Push
         {
            for (auto& weak: sessions_) {
               if (auto session = weak.lock()) {
                  session->deliver(resps_.at(3).data);
               } else {
                  std::cout << "Session expired." << std::endl;
               }
            }

         } break;
         default: { /* Ignore */ }
      }

      resps_.clear();
   }

   auto get_extended_adapter()
   {
      return [adapter = adapt(resps_)](command, type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec) mutable
         { return adapter(t, aggregate_size, depth, data, size, ec); };
   }

   auto add(std::shared_ptr<user_session_base> session)
      { sessions_.push_back(session); }
};

net::awaitable<void> run(std::shared_ptr<client> db, std::shared_ptr<receiver> recv)
{
   try {
      db->set_stream(co_await connect());
      db->send(command::hello, 3);
      db->send(command::subscribe, "channel");

      for (auto adapter = recv->get_extended_adapter();;) {
         auto const cmd = co_await db->async_read(adapter, net::use_awaitable);
         recv->on_message(cmd);
      }

   } catch (std::exception const& e) {
      db->stop_writer();
      std::cerr << "Error: " << e.what() << std::endl;
   }
}

net::awaitable<void> listener()
{
   auto ex = co_await net::this_coro::executor;
   net::ip::tcp::acceptor acceptor(ex, {net::ip::tcp::v4(), 55555});

   auto recv = std::make_shared<receiver>();
   auto db = std::make_shared<client>(ex);

   net::co_spawn(ex, run(db, recv), net::detached);

   auto on_user_msg = [db](std::string const& msg)
   {
      db->send(command::publish, "channel", msg);
      db->send(command::incr, "message-counter");
   };

   for (;;) {
      auto socket = co_await acceptor.async_accept(net::use_awaitable);
      auto session = std::make_shared<user_session>(std::move(socket));
      recv->add(session);
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
