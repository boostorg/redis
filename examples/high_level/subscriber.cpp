/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;

using aedis::redis::command;
using aedis::resp3::node;
using aedis::generic::receiver_base;
using aedis::generic::client;
using client_type = client<net::ip::tcp::socket, command>;
using response_type = std::vector<node<std::string>>;

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

class myreceiver : public receiver_base<command, response_type> {
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

