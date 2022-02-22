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
using client_type = client<aedis::net::ip::tcp::socket>;

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

class receiver {
private:
   std::vector<node<std::string>> resps_;
   std::shared_ptr<client_type> db_;

public:
   receiver(std::shared_ptr<client_type> db) : db_{db} {}

   void operator()(command cmd)
   {
      switch (cmd) {
         case command::hello:
         db_->send(command::subscribe, "channel1", "channel2");
         break;

         case command::unknown:
         std::cout
            << "Event: " << resps_.at(1).value << "\n"
            << "Channel: " << resps_.at(2).value << "\n"
            << "Message: " << resps_.at(3).value << "\n"
            << std::endl;
         break;

         default:;
      }

      resps_.clear();
   }

   auto adapter() { return redis::adapt(resps_); }
};

int main()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());

   receiver recv{db};
   db->set_response_adapter(recv.adapter());
   db->set_reader_callback(std::ref(recv));

   db->async_run({net::ip::make_address("127.0.0.1"), 6379}, [](auto){});
   ioc.run();
}

