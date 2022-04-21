/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <iostream>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::resp3::node;
using aedis::adapter::adapter_t;
using aedis::adapter::adapt;
using aedis::redis::command;
using aedis::generic::client;

using client_type = client<net::ip::tcp::socket, command>;
using response_type = node<std::string>;

struct receiver {
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
         db_->send(command::ping, "O rato roeu a roupa do rei de Roma");
         db_->send(command::incr, "intro-counter");
         db_->send(command::set, "intro-key", "Três pratos de trigo para três tigres");
         db_->send(command::get, "intro-key");
         db_->send(command::quit);
         break;

         default:
         std::cout << resp_.value << std::endl;
      }
   }

   void on_write(std::size_t n)
   { 
      std::cout << "Number of bytes written: " << n << std::endl;
   }

private:
   response_type resp_;
   adapter_t<response_type> adapter_;
   client_type* db_;
};

int main()
{
   net::io_context ioc;

   client_type db(ioc.get_executor());
   receiver recv{db};
   db.set_read_handler([&recv](command cmd, std::size_t n){recv.on_read(cmd, n);});
   db.set_write_handler([&recv](std::size_t n){recv.on_write(n);});
   db.set_resp3_handler([&recv](command cmd, auto const& nd, auto& ec){recv.on_resp3(cmd, nd, ec);});

   db.async_run("127.0.0.1", "6379",
      [](auto ec){ std::cout << ec.message() << std::endl;});

   ioc.run();
}

