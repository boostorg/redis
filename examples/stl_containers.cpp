/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <map>
#include <vector>
#include <memory>
#include <iostream>
#include <unordered_map>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "lib/net_utils.hpp"

namespace net = aedis::net;
namespace redis = aedis::redis;
using aedis::redis::command;
using aedis::resp3::node;
using client_type = redis::client<aedis::net::ip::tcp::socket>;

void send_containers(std::shared_ptr<client_type> db)
{
   std::map<std::string, std::string> map
      { {"key1", "value1"}
      , {"key2", "value2"}
      , {"key3", "value3"}
      };

   std::vector<int> vec
      {1, 2, 3, 4, 5, 6};

   std::set<std::string> set
      {"one", "two", "three", "four"};

   // Sends the stl containres.
   db->send_range(command::hset, "hset-key", std::cbegin(map), std::cend(map));
   db->send_range(command::rpush, "rpush-key", std::cbegin(vec), std::cend(vec));
   db->send_range(command::sadd, "sadd-key", std::cbegin(set), std::cend(set));

   // Retrieves the containers.
   db->send(command::hgetall, "hset-key");
   db->send(command::lrange, "rpush-key", 0, -1);
   db->send(command::smembers, "sadd-key");
}

// Grouping of all expected responses in a tuple.
using tuple_type =
   std::tuple<
      std::list<int>, // lrange
      std::set<std::string>, // smembers
      std::vector<node<std::string>> // Everything else.
   >;

// Maps commands in a specific tuple element.
constexpr auto to_tuple_index(command cmd)
{
   switch (cmd) {
      case command::lrange: return 0;
      case command::smembers: return 1;
      default: return 2;
   }
}

class receiver {
private:
   std::shared_ptr<client_type> db_;
   tuple_type resps_;

public:
   receiver(std::shared_ptr<client_type> db) : db_{db} {}

   auto adapter()
      { return redis::adapt2(resps_, [](command cmd){ return to_tuple_index(cmd);}); }

   void operator()(command cmd)
   {
      switch (cmd) {
         case command::hello:
         {
            send_containers(db_);
            db_->send(command::quit);
         } break;

         case command::hgetall:
         break;

         case command::lrange:
         {
            auto& cont = std::get<to_tuple_index(command::lrange)>(resps_);
            aedis::print_and_clear(cont);
         } break;

         case command::smembers:
         {
            auto& cont = std::get<to_tuple_index(command::smembers)>(resps_);
            aedis::print_and_clear(cont);
         } break;

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

   db->async_run({net::ip::make_address("127.0.0.1"), 6379}, [](auto){});
   ioc.run();
}
