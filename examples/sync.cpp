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

namespace resp3 = aedis::resp3;
using aedis::redis::command;
using aedis::redis::make_serializer;
using aedis::resp3::adapt;

namespace net = aedis::net;
using net::dynamic_buffer;
using net::ip::tcp;

int main()
{
   try {
      net::io_context ioc;
      tcp::resolver resv{ioc};
      auto const res = resv.resolve("127.0.0.1", "6379");
      tcp::socket socket{ioc};
      connect(socket, res);

      // Creates and sends the request to redis.
      std::string request;
      auto sr = make_serializer(request);
      sr.push(command::hello, 3);
      sr.push(command::ping);
      sr.push(command::quit);
      net::write(socket, net::buffer(request));

      // Will store the response to ping.
      std::string resp;

      // Reads the responses to all commands in the request.
      std::string buffer;
      resp3::read(socket, dynamic_buffer(buffer)); // hello (ignored)
      resp3::read(socket, dynamic_buffer(buffer), adapt(resp));
      resp3::read(socket, dynamic_buffer(buffer)); // quit (ignored)

      std::cout << "Ping: " << resp << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}
