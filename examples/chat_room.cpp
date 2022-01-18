/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <unordered_map>
#include <vector>

#include <aedis/src.hpp>
#include <aedis/aedis.hpp>

#include "lib/client.hpp"
#include "lib/user_session.hpp"
#include "src.hpp"

namespace net = aedis::net;
using aedis::command;
using aedis::user_session;
using aedis::user_session_base;
using aedis::resp3::client;
using aedis::resp3::node;
using aedis::resp3::response_traits;
using aedis::resp3::type;
using aedis::resp3::adapt;

struct adapter_wrapper {
   response_traits<std::vector<node>>::adapter_type push_adapter;
   response_traits<int>::adapter_type int_adapter;

   void
   operator()(
      command cmd,
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t size,
      std::error_code& ec)
   {
      // Handles only the commands we are interested in and ignores
      // the rest.
      switch (cmd) {
         case command::unknown: push_adapter(t, aggregate_size, depth, data, size, ec); return;
	 case command::incr: int_adapter(t, aggregate_size, depth, data, size, ec); return;
         default: {} // Ignore.
      }
   }
};

class my_redis_client : public client {
private:
   // Objects to hold the responses.
   std::vector<node> resp_push_;
   int resp_int_;

   // Store sessions on a vector for fast traversal.
   std::vector<std::weak_ptr<user_session_base>> sessions_;

   void on_message(command cmd) override
   {
      switch (cmd) {
	 case command::incr:
	 {
	    std::cout << "Message so far: " << resp_int_ << std::endl;
	 } break;
         default: { /* Ignore */ }
      }
   }
   void on_push() override
   {
      // TODO: Delete sessions lazily on traversal.
      for (auto& weak: sessions_) {
	 if (auto session = weak.lock()) {
	    session->deliver(resp_push_.at(3).data);
	 } else {
	    std::cout << "Session expired." << std::endl;
	 }
      }

      resp_push_.clear();
   }

public:
   my_redis_client(net::any_io_executor ex)
   : client(ex, adapter_wrapper{adapt(resp_push_), adapt(resp_int_)})
   {}

   auto subscribe(std::shared_ptr<user_session_base> session)
      { sessions_.push_back(session); }
};

net::awaitable<void> listener()
{
   auto ex = co_await net::this_coro::executor;
   net::ip::tcp::acceptor acceptor(ex, {net::ip::tcp::v4(), 55555});

   auto client = std::make_shared<my_redis_client>(ex);
   client->start();
   client->send(command::subscribe, "channel");

   for (;;) {
      auto socket = co_await acceptor.async_accept(net::use_awaitable);
      auto session = std::make_shared<user_session>(std::move(socket));
      client->subscribe(session);

      auto on_msg = [client](std::string const& msg)
      {
         client->send(command::publish, "channel", msg);
         client->send(command::incr, "message-counter");
      };

      session->start(on_msg);
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
