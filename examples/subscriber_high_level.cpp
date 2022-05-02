/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
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

class receiver {
public:
   receiver(client_type& db)
   : adapter_{adapt(resp_)}
   , db_{&db} {}

   void on_resp3(command cmd, node<boost::string_view> const& nd, boost::system::error_code& ec)
   {
      adapter_(nd, ec);
   }

   void on_read(command cmd, std::size_t)
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

   void on_push(std::size_t)
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
   auto recv = std::make_shared<receiver>(db);
   db.set_receiver(recv);

   db.async_run([](auto ec){ std::cout << ec.message() << std::endl;});

   ioc.run();
}
