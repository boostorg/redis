/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <string>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = aedis::net;
namespace redis = aedis::redis;

using redis::command;
using redis::receiver_base;
using redis::index_of;
using client_type = redis::client<net::detached_t::as_default_on_t<net::ip::tcp::socket>>;
using tuple_type = std::tuple<int, std::string>;

struct receiver : receiver_base<tuple_type> {
private:
   client_type* db_;
   tuple_type resps_;

   int to_tuple_index(command cmd) override
   {
      switch (cmd) {
         case command::incr:
         return index_of<int, tuple_type>();

         case command::ping:
         case command::quit:
         return index_of<std::string, tuple_type>();

         default:
         return -1;
      }
   }

public:
   receiver(client_type& db) : receiver_base(resps_), db_{&db} {}

   void on_read(command cmd) override
   {
      switch (cmd) {
         case command::hello:
         db_->send(command::ping, "O rato roeu a roupa do rei de Roma");
         db_->send(command::incr, "redis-client-counter");
         db_->send(command::quit);
         break;

         case command::quit:
         case command::ping:
         std::cout << std::get<std::string>(resps_) << std::endl;
         break;

         case command::incr:
         std::cout << std::get<int>(resps_) << std::endl;
         break;

         default:;
      }
   }
};

int main()
{
   net::io_context ioc;
   client_type db(ioc.get_executor());
   receiver recv{db};
   db.async_run(recv);
   ioc.run();
}

