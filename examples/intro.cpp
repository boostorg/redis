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

namespace net = aedis::net;
namespace redis = aedis::redis::experimental;
using aedis::redis::command;
using aedis::redis::experimental::client;
using aedis::resp3::node;
using client_type = client<aedis::net::ip::tcp::socket>;

class receiver {
private:
   std::vector<node> resps_;
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
         default:;
      }

      std::cout << cmd << " " << resps_.at(0).data << std::endl;
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

   db->async_run("localhost", "6379", [](auto){});
   ioc.run();
}
