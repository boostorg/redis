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
using tuple_type = std::tuple<std::vector<node<std::string>>>;

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

class receiver : public redis::receiver_tuple<tuple_type> {
private:
   std::shared_ptr<client_type> db_;
   tuple_type resps_;

public:
   receiver(std::shared_ptr<client_type> db)
   : receiver_tuple(resps_), db_{db} {}

   void on_read(command cmd) override
   {
      switch (cmd) {
         case command::unknown:
         std::cout
            << "Event: " << std::get<0>(resps_).at(1).value << "\n"
            << "Channel: " << std::get<0>(resps_).at(2).value << "\n"
            << "Message: " << std::get<0>(resps_).at(3).value << "\n"
            << std::endl;
         break; default:;
      }

      std::get<0>(resps_).clear();
   }
};

int main()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());
   db->send(command::subscribe, "channel1", "channel2");
   receiver recv{db};
   db->async_run(recv);
   ioc.run();
}

