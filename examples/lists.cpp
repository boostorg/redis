/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <deque>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "lib/net_utils.hpp"

namespace net = aedis::net;
using aedis::redis::command;
using aedis::redis::experimental::client;
using aedis::redis::experimental::adapt;

// From lib/net_utils.hpp
using aedis::connection_manager;
using socket_type = aedis::net::use_awaitable_t<>::as_default_on_t<aedis::net::ip::tcp::socket>;
using client_type = client<socket_type>;

net::awaitable<void> reader(std::shared_ptr<client_type> db)
{
   auto vec  = {1, 2, 3, 4, 5, 6};

   // Enqueue the commands.
   db->send(command::hello, 3);
   db->send(command::flushall);
   db->send_range(command::rpush, "key", std::cbegin(vec), std::cend(vec));
   db->send(command::lrange, "key", 0, -1);
   db->send(command::lrange, "key", 0, -1);
   db->send(command::lrange, "key", 0, -1);
   db->send(command::lrange, "key", 0, -1);
   db->send(command::quit);

   // Expected responses.
   int rpush;
   std::vector<std::string> svec;
   std::list<std::string> slist;
   std::deque<std::string> sdeq;
   std::vector<int> ivec;

   // Reads the responses.
   co_await db->async_read(); // hello
   co_await db->async_read(); // flushall
   co_await db->async_read(adapt(rpush)); // rpush
   co_await db->async_read(adapt(svec));
   co_await db->async_read(adapt(slist));
   co_await db->async_read(adapt(sdeq));
   co_await db->async_read(adapt(ivec));
   co_await db->async_read(); // quit

   // Reads eof (caused by the quit command).
   boost::system::error_code ec;
   co_await db->async_read(adapt(), net::redirect_error(net::use_awaitable, ec));

   // Prints the responses.
   std::cout << "rpush: " << rpush;
   std::cout << "\nlrange (as vector): ";
   for (auto e: svec) std::cout << e << " ";
   std::cout << "\nlrange (as list): ";
   for (auto e: slist) std::cout << e << " ";
   std::cout << "\nlrange (as deque): ";
   for (auto e: sdeq) std::cout << e << " ";
   std::cout << "\nlrange (as vector<int>): ";
   for (auto e: ivec) std::cout << e << " ";
   std::cout << std::endl;
}

int main()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());
   net::co_spawn(ioc, connection_manager(db, reader(db)), net::detached);
   ioc.run();
}
