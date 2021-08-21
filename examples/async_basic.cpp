/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>
#include <aedis/detail/utils.hpp>

using namespace aedis;

void print_helper(command cmd, resp3::type type, buffers& buf)
{
   switch (type) {
      case resp3::type::simple_string:
      {
         std::cout << cmd << " " << buf.simple_string << " (" << type << ")" << std::endl;
      } break;
      case resp3::type::push:
      case resp3::type::map:
      {
         std::cout << cmd << " (" << type << ")" << std::endl;
      } break;
      default:{}
   }
}

struct myreceiver {
   std::shared_ptr<connection> conn;
   buffers& buf;

   void operator()(command cmd, resp3::type type) const
   {
      if (cmd == command::hello) {
         assert(type == resp3::type::map);
         conn->ping();
         conn->psubscribe({"aaa*"});
         conn->quit();
      }

      print_helper(cmd, type, buf);
   }
};

int main()
{
   net::io_context ioc {1};
   auto conn = std::make_shared<connection>(ioc.get_executor());

   buffers bufs;
   myreceiver recv{conn, bufs};
   conn->run(std::ref(recv), bufs);
   ioc.run();
}
