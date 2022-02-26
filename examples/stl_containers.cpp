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
using aedis::redis::receiver_base;
using aedis::redis::index_of;
using aedis::resp3::node;
using client_type = redis::client<net::detached_t::as_default_on_t<aedis::net::ip::tcp::socket>>;

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
      std::list<int>,
      std::set<std::string>,
      std::vector<node<std::string>>
   >;

struct receiver : receiver_base<tuple_type> {
private:
   std::shared_ptr<client_type> db_;
   tuple_type resps_;

  int to_tuple_index(command cmd) override
  {
     switch (cmd) {
        case command::lrange:
        return index_of<std::list<int>, tuple_type>();

        case command::smembers:
        return index_of<std::set<std::string>, tuple_type>();

        default:
        return -1;
     }
  }

public:
   receiver(std::shared_ptr<client_type> db) : receiver_base(resps_), db_{db} {}

   void on_read(command cmd) override
   {
      switch (cmd) {
         case command::hello:
         send_containers(db_);
         db_->send(command::quit);
         break;

         case command::lrange:
         aedis::print_and_clear(std::get<std::list<int>>(resps_));
         break;

         case command::smembers:
         aedis::print_and_clear(std::get<std::set<std::string>>(resps_));
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
   db->async_run(recv);
   ioc.run();
}
