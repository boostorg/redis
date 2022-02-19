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

net::awaitable<void> connection_manager()
{
   using namespace net::experimental::awaitable_operators;

   try {
     auto ex = co_await net::this_coro::executor;
     auto db = std::make_shared<client_type>(ex);

     // Enqueue the commands.
     db->send(command::hello, 3);
     db->send(command::ping, "O rato roeu a roupa do rei de Roma");
     db->send(command::incr, "redis-client-counter");
     db->send(command::quit);
   
     std::vector<node> resps;

     auto f = [&resps] (command cmd) mutable
        { std::cout << cmd << " " << resps.at(0).data << std::endl; resps.clear(); };

     db->set_reader_callback(f);
     db->set_response_adapter(redis::adapt(resps));

     resolver_type resolver{ex};
     auto const res = co_await resolver.async_resolve("localhost", "6379");
     co_await net::async_connect(db->next_layer(), std::cbegin(res), std::end(res));
     co_spawn(ex, db->async_writer(), net::detached);
     boost::system::error_code ec;
     co_await db->async_reader(net::redirect_error(net::use_awaitable, ec));
     std::clog << ec.message() << std::endl;

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
