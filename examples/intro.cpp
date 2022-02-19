/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <memory>

#include <boost/asio/experimental/awaitable_operators.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = aedis::net;
namespace redis = aedis::redis::experimental;
using aedis::redis::command;
using aedis::redis::experimental::client;
using aedis::resp3::node;

using resolver_type = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::ip::tcp::resolver>;
using socket_type = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::ip::tcp::socket>;
using client_type = client<socket_type>;

class receiver {
private:
   std::vector<node> resps_;
   std::shared_ptr<client_type> db_;

public:
   receiver(std::shared_ptr<client_type> db) : db_{db} {}

   void operator()(command cmd)
   {
      switch (cmd) {
         case command::hello:
         db_->send(command::ping, "O rato roeu a roupa do rei de Roma");
         db_->send(command::incr, "redis-client-counter");
         db_->send(command::quit);
         default:;
      }

      std::cout << cmd << " " << resps_.at(0).data << std::endl;
      resps_.clear();
   }

   auto adapter() { return redis::adapt(resps_); }
};

net::awaitable<void> connection_manager()
{
   using namespace net::experimental::awaitable_operators;

   try {
     auto ex = co_await net::this_coro::executor;
     auto db = std::make_shared<client_type>(ex);

     receiver recv{db};
     db->set_response_adapter(recv.adapter());
     db->set_reader_callback(std::ref(recv));

     co_await db->async_connect();
     co_await (db->async_reader() && db->async_writer());

   } catch (std::exception const& e) {
      std::clog << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   net::co_spawn(ioc, connection_manager(), net::detached);
   ioc.run();
}
