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

using aedis::resp3::node;
using aedis::sentinel::command;
using aedis::generic::client;
using aedis::adapter::adapt;
using client_type = client<net::ip::tcp::socket, command>;
using response_type = std::vector<node<std::string>>;
using adapter_type = aedis::adapter::adapter_t<response_type>;

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

class myreceiver {
public:
   myreceiver(client_type& db)
   : adapter_{adapt(resp_)}
   , db_{&db} {}

   void on_resp3(command cmd, node<boost::string_view> const& nd, boost::system::error_code& ec)
   {
      adapter_(nd, ec);
   }

   void on_read(command cmd)
   {
      switch (cmd) {
         case command::hello:
         db_->send(command::subscribe, "channel1", "channel2");
         break;
         default:;
      }

      resp_.clear();
   }

   void on_write(std::size_t n)
   { 
      std::cout << "Number of bytes written: " << n << std::endl;
   }

   void on_push()
   {
      std::cout
         << "Event: " << resp_.at(1).value << "\n"
         << "Channel: " << resp_.at(2).value << "\n"
         << "Message: " << resp_.at(3).value << "\n"
         << std::endl;

      resp_.clear();
   }

private:
   response_type resp_;
   adapter_type adapter_;
   client_type* db_;
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
