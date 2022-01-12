/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/src.hpp>
#include <aedis/aedis.hpp>

namespace resp3 = aedis::resp3;
using aedis::command;
using aedis::resp3::serializer;
using aedis::resp3::adapt;
using aedis::resp3::node;

namespace net = aedis::net;
using net::ip::tcp;
using net::buffer;
using net::dynamic_buffer;

/* Aedis supports synchronous communication too.
 */

int main()
{
   try {
     net::io_context ioc;
     tcp::resolver resv{ioc};
     auto const res = resv.resolve("127.0.0.1", "6379");
     tcp::socket socket{ioc};
     connect(socket, res);

     serializer<command> sr;
     sr.push(command::hello, 3);
     sr.push(command::command);
     sr.push(command::quit);
     net::write(socket, buffer(sr.request()));

     std::vector<node> resp;

     std::string buffer;
     resp3::read(socket, dynamic_buffer(buffer));
     resp3::read(socket, dynamic_buffer(buffer), adapt(resp));
     resp3::read(socket, dynamic_buffer(buffer));

     std::cout << resp << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
