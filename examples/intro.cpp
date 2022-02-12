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

#include "lib/net_utils.hpp"

namespace net = aedis::net;
using aedis::redis::command;
using aedis::redis::experimental::client;
using aedis::redis::experimental::adapt;

net::awaitable<void> reader(std::shared_ptr<client> db)
{
   db->send(command::hello, 3);
   db->send(command::ping, "O rato roeu a roupa do rei de Roma");
   db->send(command::incr, "redis-client-counter");
   db->send(command::quit);
   
   std::string ping;
   int incr;
   
   co_await db->async_read();
   co_await db->async_read(adapt(ping));
   co_await db->async_read(adapt(incr));
   co_await db->async_read();

   boost::system::error_code ec;
   co_await db->async_read(adapt(), net::redirect_error(net::use_awaitable, ec));
   
   std::cout
      << "ping: " << ping << "\n"
      << "incr: " << incr << "\n";
}

net::awaitable<void>
connection_manager()
{
   using namespace net::experimental::awaitable_operators;

   auto ex = co_await net::this_coro::executor;
   auto db = std::make_shared<client>(ex);
   db->set_stream(co_await connect());
   co_await (reader(db) || writer(db));
}

int main()
{
   try {
      net::io_context ioc{1};
      net::co_spawn(ioc, connection_manager(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}
