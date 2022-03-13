/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres.gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <algorithm>
#include <cctype>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace resp3 = aedis::resp3;
using aedis::redis::command;
using aedis::redis::make_serializer;
using resp3::adapt;
using resp3::node;

namespace net = aedis::net;
using net::ip::tcp;
using net::write;
using net::buffer;
using net::dynamic_buffer;

int main()
{
   try {
     net::io_context ioc;
     tcp::resolver resv{ioc};
     auto const res = resv.resolve("127.0.0.1", "6379");
     tcp::socket socket{ioc};
     connect(socket, res);

     std::string request;
     auto sr = make_serializer(request);
     sr.push(command::hello, 3);
     sr.push(command::command);
     sr.push(command::quit);
     write(socket, buffer(request));

     std::vector<node<std::string>> resp;

     std::string buffer;
     resp3::read(socket, dynamic_buffer(buffer));
     resp3::read(socket, dynamic_buffer(buffer), adapt(resp));
     resp3::read(socket, dynamic_buffer(buffer));

     std::cout << resp << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

