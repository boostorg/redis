/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <vector>
#include <iostream>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = aedis::net;
using aedis::redis::command;
using aedis::redis::receiver;
using client_type = aedis::redis::client<net::ip::tcp::socket>;
using response_type = aedis::resp3::node<std::string>;

struct myreceiver : receiver<response_type> {
public:
   myreceiver(client_type& db): db_{&db} {}

private:
   client_type* db_;

   void on_read_impl(command cmd) override
   {
      switch (cmd) {
         case command::hello:
         db_->send(command::ping, "O rato roeu a roupa do rei de Roma");
         db_->send(command::incr, "intro-counter");
         db_->send(command::set, "intro-key", "Três pratos de trigo para três tigres");
         db_->send(command::get, "intro-key");
         db_->send(command::quit);
         break;

         default:
         std::cout << get<response_type>().value << std::endl;
      }
   }
};

int main()
{
   net::io_context ioc;
   client_type db(ioc.get_executor());
   myreceiver recv{db};

   db.async_run(
       recv,
       {net::ip::make_address("127.0.0.1"), 6379},
       [](auto ec){ std::cout << ec.message() << std::endl;});

   ioc.run();
}

