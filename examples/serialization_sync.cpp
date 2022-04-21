/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <iterator>
#include <cstdint>
#include <iostream>
#include <algorithm>

#include <boost/asio/connect.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "mystruct.hpp"

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using aedis::resp3::type;
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

      // This struct will be serialized and stored on Redis.
      mystruct in{42, "Some string"};

      // Creates and sends a request to redis.
      std::string request;
      auto sr = make_serializer(request);
      sr.push(command::hello, 3);
      sr.push(command::set, "key", in);
      sr.push(command::get, "key");
      sr.push(command::quit);
      net::write(socket, net::buffer(request));

      // Object to store the response.
      mystruct out;

      // Reads the responses to all commands in the request.
      std::string buffer;
      resp3::read(socket, dynamic_buffer(buffer)); // hello
      resp3::read(socket, dynamic_buffer(buffer)); // set
      resp3::read(socket, dynamic_buffer(buffer), adapt(out)); // get
      resp3::read(socket, dynamic_buffer(buffer)); // quit

      // Should be equal to what has been sent above.
      std::cout << out << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}
