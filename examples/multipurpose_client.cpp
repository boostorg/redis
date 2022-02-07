/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <memory>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "lib/net_utils.hpp"

namespace net = aedis::net;
using aedis::redis::command;
using aedis::resp3::experimental::client;
using aedis::resp3::node;
using aedis::resp3::type;

net::awaitable<void> connection_manager(std::shared_ptr<client> db)
{
   try {
      auto socket = co_await connect();
      co_await db->engage(std::move(socket));
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}

int main()
{
   try {
      std::vector<node> resps;

      auto on_msg = [&resps](std::error_code ec, command cmd)
      {
         if (ec) {
            std::cerr << "Error: " << ec.message() << std::endl;
            return;
         }

         std::cout << cmd << ":: " << resps.front().data << std::endl;
         resps.clear();
      };

      net::io_context ioc{1};

      // This adapter uses the general response that is suitable for
      // all commands, so the command parameter will be ignored.
      auto ext_adapter = [adapter = adapt(resps)](command, type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec) mutable
         { return adapter(t, aggregate_size, depth, data, size, ec); };

      auto db = std::make_shared<client>(ioc.get_executor());
      db->set_extended_adapter(ext_adapter);
      db->set_msg_callback(on_msg);
      net::co_spawn(ioc, connection_manager(db), net::detached);

      db->send(command::ping, "O rato roeu a roupa do rei de Roma");
      db->send(command::incr, "redis-client-counter");
      db->send(command::quit);

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}
