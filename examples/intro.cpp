/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <string>
#include <memory>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = aedis::net;
namespace redis = aedis::redis;
using aedis::redis::command;
using aedis::redis::client;
using aedis::resp3::node;
using client_type = client<aedis::net::ip::tcp::socket>;

class receiver {
private:
   node<std::string> resps_;
   std::shared_ptr<client_type> db_;

public:
   receiver(std::shared_ptr<client_type> db) : db_{db} {}

   void operator()(command cmd)
   {
      switch (cmd) {
         case command::hello:
         db_->send(command::ping, "O rato roeu a roupa do rei de Roma");
         db_->send(command::incr, "redis-client-counter");
         db_->send(command::quit);
         break;

         case command::ping:
         std::cout << "Ping message: " << resps_.value << std::endl;
         break;

         case command::incr:
         std::cout << "Ping counter: " << resps_.value << std::endl;
         break;

         case command::quit:
         std::cout << command::quit << ": " << resps_.value << std::endl;
         break;

         default:;
      }

      resps_.value.clear();
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
