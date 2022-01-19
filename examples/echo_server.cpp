/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

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
using aedis::resp3::adapt;
using aedis::resp3::response_traits;
using aedis::resp3::type;

// Uses one adapter for each different command.
struct adapter_wrapper {
   response_traits<std::string>::adapter_type str_adapter;
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
	 case command::ping: str_adapter(t, aggregate_size, depth, data, size, ec); return;
	 case command::incr: int_adapter(t, aggregate_size, depth, data, size, ec); return;
         default: {} // Ignore.
      }
   }
};

class db : public std::enable_shared_from_this<db> {
private:
   std::string resp_str_;
   int resp_int_;
   std::queue<std::weak_ptr<user_session_base>> sessions_;

public:
   auto get_adapter()
      { return adapter_wrapper{adapt(resp_str_), adapt(resp_int_)}; }

   void add_user_session(std::shared_ptr<user_session_base> session)
      { sessions_.push(session); }

   void on_message(command cmd, std::shared_ptr<client>)
   {
      switch (cmd) {
	 case command::ping:
	 {
	    if (auto session = sessions_.front().lock()) {
	       session->deliver(resp_str_);
	    } else {
	       std::cout << "Session expired." << std::endl;
	    }

	    sessions_.pop();
	    resp_str_.clear();
	 } break;
	 case command::incr:
	 {
	    std::cout << "Echos so far: " << resp_int_ << std::endl;
	 } break;
         default: { assert(false); }
      }
   }
};

net::awaitable<void> listener()
{
   auto ex = co_await net::this_coro::executor;
   net::ip::tcp::acceptor acceptor(ex, {net::ip::tcp::v4(), 55555});
   
   auto rdb = std::make_shared<db>();
   auto on_message = [rdb](command cmd, std::shared_ptr<client> cl)
      { rdb->on_message(cmd, cl); };

   auto redis = std::make_shared<client>(ex, rdb->get_adapter(), on_message);
   redis->start();

   for (;;) {
      auto socket = co_await acceptor.async_accept(net::use_awaitable);
      auto session = std::make_shared<user_session>(std::move(socket));

      auto on_msg = [redis, rdb, session](std::string const& msg)
      {
	 redis->send(command::ping, msg);
	 rdb->add_user_session(session);
	 redis->send(command::incr, "echo-counter");
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
