/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <iostream>

#include <boost/asio/connect.hpp>

#include <aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using aedis::resp3::request;
using aedis::adapter::adapt2;
using net::ip::tcp;

int main()
{
   try {
      net::io_context ioc;
      tcp::resolver resv{ioc};
      auto const res = resv.resolve("127.0.0.1", "6379");
      tcp::socket socket{ioc};
      net::connect(socket, res);

      // Creates the request and writes to the socket.
      request req;
      req.push("HELLO", 3);
      req.push("PING", "Hello world");
      req.push("QUIT");
      resp3::write(socket, req);

      // Responses
      std::string buffer, resp;

      // Reads the responses to all commands in the request.
      auto dbuffer = net::dynamic_buffer(buffer);
      resp3::read(socket, dbuffer);
      resp3::read(socket, dbuffer, adapt2(resp));
      resp3::read(socket, dbuffer);

      std::cout << "Ping: " << resp << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}
