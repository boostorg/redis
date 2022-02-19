/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <set>
#include <vector>
#include <iostream>
#include <unordered_map>

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
   std::set<std::string> set
      {"one", "two", "three", "four"};

   // Enqueue the commands.
   db->send(command::hello, 3);
   db->send(command::flushall);
   db->send_range(command::sadd, "key", std::cbegin(set), std::cend(set));
   db->send(command::smembers, "key");
   db->send(command::smembers, "key");
   db->send(command::smembers, "key");
   db->send(command::quit);

   // Expected responses.
   int sadd;
   std::vector<std::string> smembers1;
   std::set<std::string> smembers2;
   std::unordered_set<std::string> smembers3;

   // Reads the responses.
   co_await db->async_read(); // hello
   co_await db->async_read(); // flushall
   co_await db->async_read(adapt(sadd));
   co_await db->async_read(adapt(smembers1));
   co_await db->async_read(adapt(smembers2));
   co_await db->async_read(adapt(smembers3));
   co_await db->async_read(); // quit

   // Reads eof (caused by the quit command).
   boost::system::error_code ec;
   co_await db->async_read(adapt(), net::redirect_error(net::use_awaitable, ec));
   
   // Prints the responses.
   std::cout << "sadd: " << sadd;
   std::cout << "\nsmembers (as vector): ";
   for (auto const& e: smembers1) std::cout << e << " ";
   std::cout << "\nsmembers (as set): ";
   for (auto const& e: smembers2) std::cout << e << " ";
   std::cout << "\nsmembers (as unordered_set): ";
   for (auto const& e: smembers3) std::cout << e << " ";
   std::cout << "\n";
}

int main()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());
   net::co_spawn(ioc, connection_manager(db, reader(db)), net::detached);
   ioc.run();
}
