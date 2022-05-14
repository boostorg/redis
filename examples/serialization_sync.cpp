/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
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
using aedis::generic::serializer;
using aedis::adapter::adapt;
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
      serializer<command> req;
      req.push(command::hello, 3);
      req.push(command::set, "key", in);
      req.push(command::get, "key");
      req.push(command::quit);
      resp3::write(socket, req);

      // Object to store the response.
      mystruct out;

      // Reads the responses to all commands in the request.
      std::string buffer;
      auto dbuf = net::dynamic_buffer(buffer);
      resp3::read(socket, dbuf); // hello
      resp3::read(socket, dbuf); // set
      resp3::read(socket, dbuf, adapt(out)); // get
      resp3::read(socket, dbuf); // quit

      // Should be equal to what has been sent above.
      std::cout << out << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}
