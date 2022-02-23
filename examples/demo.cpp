/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <string>
#include <memory>

#include <boost/mp11.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = aedis::net;
namespace redis = aedis::redis;
using aedis::redis::command;
using client_type = redis::client<aedis::net::ip::tcp::socket>;

// Groups expected responses in a tuple.
using tuple_type = std::tuple<int, std::string>;

// Maps each command into a tuple element. Use -1 to ignore a response.
constexpr auto to_tuple_index(command cmd)
{
   switch (cmd) {
      case command::incr: return boost::mp11::mp_find<tuple_type, int>::value;
      case command::ping:
      case command::quit: return boost::mp11::mp_find<tuple_type, std::string>::value;
      default: return std::tuple_size<tuple_type>::value;
   }
}

// Receives commands from the Redis server.
class receiver {
private:
   tuple_type resps_;
   std::shared_ptr<client_type> db_;

public:
   receiver(std::shared_ptr<client_type> db) : db_{db} {}

   auto adapter()
      { return redis::adapt2(resps_, [](command cmd){ return to_tuple_index(cmd);}); }

   void operator()(command cmd)
   {
      switch (cmd) {
         case command::hello:
         db_->send(command::ping, "O rato roeu a roupa do rei de Roma");
         db_->send(command::incr, "redis-client-counter");
         db_->send(command::quit);
         break;

         case command::ping:
         std::cout << "Ping message: " << std::get<to_tuple_index(command::ping)>(resps_) << std::endl;
         break;

         case command::incr:
         std::cout << "Ping counter: " << std::get<to_tuple_index(command::incr)>(resps_) << std::endl;
         break;

         case command::quit:
         std::cout << command::quit << ": " << std::get<to_tuple_index(command::quit)>(resps_) << std::endl;
         break;

         default:;
      }
   }
};

int main()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());

   receiver recv{db};
   db->set_response_adapter(recv.adapter());
   db->set_reader_callback(std::ref(recv));

   net::ip::tcp::endpoint ep{net::ip::make_address("127.0.0.1"), 6379};

   db->async_run(ep, [](auto){});
   ioc.run();
}
