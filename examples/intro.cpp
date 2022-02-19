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

net::awaitable<void> reader(std::shared_ptr<client_type> db)
{
   // Enqueue the commands.
   db->send(command::hello, 3);
   db->send(command::ping, "O rato roeu a roupa do rei de Roma");
   db->send(command::incr, "redis-client-counter");
   db->send(command::quit);
   
   // Expected responses.
   std::vector<node> resps;
   
   // Reads the responses.
   co_await db->async_read();
   co_await db->async_read(redis::adapt(resps));
   co_await db->async_read(redis::adapt(resps));
   co_await db->async_read();

   // Reads eof (caused by the quit command).
   boost::system::error_code ec;
   co_await db->async_read(redis::adapt(), net::redirect_error(net::use_awaitable, ec));
   
   std::cout
      << "ping: " << resps.at(0).data << "\n"
      << "incr: " << resps.at(1).data << "\n";
}

net::awaitable<void> connection_manager()
{
   using namespace net::experimental::awaitable_operators;

   try {
     auto ex = co_await net::this_coro::executor;
     auto db = std::make_shared<client_type>(ex);

     resolver_type resolver{ex};

     auto const res = co_await resolver.async_resolve("localhost", "6379");
     co_await net::async_connect(db->next_layer(), std::cbegin(res), std::end(res));
     co_await (db->async_writer() || reader(db));

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
