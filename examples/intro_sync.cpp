/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <iostream>

#include <boost/asio/connect.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using aedis::redis::command;
using aedis::generic::make_serializer;
using aedis::adapter::adapt;
using net::dynamic_buffer;
using net::ip::tcp;

int main()
{
   try {
      net::io_context ioc;
      tcp::resolver resv{ioc};
      auto const res = resv.resolve("127.0.0.1", "6379");
      tcp::socket socket{ioc};
      net::connect(socket, res);

      // Creates and sends a request to redis.
      std::string buffer;
      auto sr = make_serializer(buffer);
      sr.push(command::hello, 3);
      sr.push(command::ping);
      sr.push(command::quit);
      net::write(socket, net::buffer(buffer));
      buffer.clear();

      // Responses
      std::string resp;

      // Reads the responses to all commands in the request.
      auto dbuffer = dynamic_buffer(buffer);
      resp3::read(socket, dbuffer);
      resp3::read(socket, dbuffer, adapt(resp));
      resp3::read(socket, dbuffer);

      std::cout << "Ping: " << resp << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}
