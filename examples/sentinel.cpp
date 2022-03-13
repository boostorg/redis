/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = aedis::net;
using aedis::sentinel::command;
using aedis::sentinel::receiver_base;
using client_type = aedis::sentinel::client<aedis::net::ip::tcp::socket>;
using aedis::resp3::node;
using response_type = std::vector<node<std::string>>;

class myreceiver : public receiver_base<response_type> {
public:
   myreceiver(client_type& db) : db_{&db} {}

private:
   client_type* db_;

   void on_push_impl() override
   {
      std::cout
         << "Event: " << get<response_type>().at(1).value << "\n"
         << "Channel: " << get<response_type>().at(2).value << "\n"
         << "Message: " << get<response_type>().at(3).value << "\n"
         << std::endl;

      get<response_type>().clear();
   }

   void on_read_impl(command cmd) override
   {
      switch (cmd) {
         case command::hello:
         db_->send(command::subscribe, "channel1", "channel2");
         break;
         default:;
      }

      get<response_type>().clear();
   }
};

int main()
{
   net::io_context ioc;
   client_type db{ioc.get_executor()};
   myreceiver recv{db};

   db.async_run(
      recv,
      {net::ip::make_address("127.0.0.1"), 6379},
      [](auto ec){ std::cout << ec.message() << std::endl;});

   ioc.run();
}

