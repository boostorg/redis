/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "lib/net_utils.hpp"

namespace net = aedis::net;
namespace redis = aedis::redis;
using aedis::redis::command;
using aedis::redis::client;
using aedis::resp3::node;
using client_type = redis::client<net::detached_t::as_default_on_t<aedis::net::ip::tcp::socket>>;
using redis::receiver_tuple;
using response_type = std::vector<node<std::string>>;

/* In this example we send a subscription to a channel and start
 * reading server side messages indefinitely.
 *
 * After starting the example you can test it by sending messages with
 * redis-cli like this
 *
 *    $ redis-cli -3
 *    127.0.0.1:6379> PUBLISH channel1 some-message
 *    (integer) 3
 *    127.0.0.1:6379>
 *
 * The messages will then appear on the terminal you are running the
 * example.
 */

struct receiver : redis::receiver_tuple<response_type> {

   void on_read(command cmd) override
   {
      switch (cmd) {
         case command::unknown:
         std::cout
            << "Event: " << get<response_type>().at(1).value << "\n"
            << "Channel: " << get<response_type>().at(2).value << "\n"
            << "Message: " << get<response_type>().at(3).value << "\n"
            << std::endl;
         break; default:;
      }

      get<response_type>().clear();
   }
};

int main()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());
   db->send(command::subscribe, "channel1", "channel2");
   receiver recv;
   db->async_run(recv);
   ioc.run();
}

