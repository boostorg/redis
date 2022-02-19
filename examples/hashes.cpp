/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <map>
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
   std::map<std::string, std::string> map
      { {"key1", "value1"}
      , {"key2", "value2"}
      , {"key3", "value3"}
      };

   // Enqueue the requests.
   db->send(command::hello, 3);
   db->send(command::flushall);
   db->send_range(command::hset, "key", std::cbegin(map), std::cend(map));
   db->send(command::hgetall, "key");
   db->send(command::hgetall, "key");
   db->send(command::hgetall, "key");
   db->send(command::quit);

   // The expected responses
   int hset;
   std::vector<std::string> hgetall1;
   std::map<std::string, std::string> hgetall2;
   std::unordered_map<std::string, std::string> hgetall3;

   // Reads the responses.
   co_await db->async_read(); // hello
   co_await db->async_read(); // flushall
   co_await db->async_read(adapt(hset));
   co_await db->async_read(adapt(hgetall1));
   co_await db->async_read(adapt(hgetall2));
   co_await db->async_read(adapt(hgetall3));
   co_await db->async_read(); // quit

   // Reads eof (caused by the quit command).
   boost::system::error_code ec;
   co_await db->async_read(adapt(), net::redirect_error(net::use_awaitable, ec));

   // Prints the responses.
   std::cout << "hset: " << hset;
   std::cout << "\nhgetall (as vector): ";
   for (auto const& e: hgetall1) std::cout << e << ", ";
   std::cout << "\nhgetall (as map): ";
   for (auto const& e: hgetall2) std::cout << e.first << " ==> " << e.second << "; ";
   std::cout << "\nhgetall (as unordered_map): ";
   for (auto const& e: hgetall3) std::cout << e.first << " ==> " << e.second << "; ";
   std::cout << "\n";
}

int main()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());
   net::co_spawn(ioc, connection_manager(db, reader(db)), net::detached);
   ioc.run();
}
