/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <vector>

#include <aedis/src.hpp>
#include <aedis/aedis.hpp>

#include "lib/user_session.hpp"
#include "lib/responses.hpp"
#include "src.hpp"

namespace net = aedis::net;
using aedis::command;
using aedis::user_session;
using aedis::user_session_base;
using aedis::resp3::experimental::client;

class receiver : public std::enable_shared_from_this<receiver> {
private:
   responses resps_;
   std::vector<std::weak_ptr<user_session_base>> sessions_;

public:
   auto get_adapter()
      { return adapter_wrapper{resps_}; }

   auto add(std::shared_ptr<user_session_base> session)
      { sessions_.push_back(session); }

   void on_message(std::error_code ec, command cmd)
   {
      if (ec) {
	 std::cerr << "Error: " << ec.message() << std::endl;
	 return;
      }

      switch (cmd) {
	 case command::incr:
	 {
	    std::cout << "Message so far: " << resps_.number << std::endl;
	 } break;
	 case command::unknown: // Push
	 {
	    // TODO: Delete sessions lazily on traversal.
	    for (auto& weak: sessions_) {
	       if (auto session = weak.lock()) {
		  session->deliver(resps_.general.at(3).data);
	       } else {
		  std::cout << "Session expired." << std::endl;
	       }
	    }

	    resps_.general.clear();
	 } break;
         default: { /* Ignore */ }
      }
   }
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
   db->send(command::subscribe, "channel");
   db->start();

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
