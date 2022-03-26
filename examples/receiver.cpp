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

using aedis::redis::command;
using aedis::resp3::type;
using aedis::adapter::node;
using client_type = aedis::redis::client<net::detached_t::as_default_on_t<net::ip::tcp::socket>>;

struct receiver {
   client_type* db;
   std::vector<node<std::string>> resps_;

   void on_resp3(
      command, // Ignore
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t size,
      boost::system::error_code&)
   {
      resps_.push_back({t, aggregate_size, depth, std::string{data, size}});
   }

   void on_push()
   {
      std::cout << "on_push: " << std::endl;
   }

   void on_read(command cmd)
   {
      std::cout << "on_read: " << cmd << std::endl;

      switch (cmd) {
         case command::hello: db->send(command::quit); break;
         default:;
      }

      resps_.clear();
   }

   void on_write(std::size_t n)
   {
      std::cout << "on_write: " << n << std::endl;
   }
};

int main()
{
   net::io_context ioc;
   client_type db(ioc.get_executor());
   receiver recv{&db};
   db.async_run(recv);
   ioc.run();
}

