/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/read.hpp>
#include <boost/redis/detail/write.hpp>
#include <boost/redis/adapter/adapt.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#define BOOST_TEST_MODULE conn-quit
#include <boost/test/included/unit_test.hpp>
#include <string>
#include <iostream>

namespace net = boost::asio;
namespace redis = boost::redis;
using boost::redis::adapter::adapt2;
using boost::redis::request;
using boost::redis::adapter::result;

BOOST_AUTO_TEST_CASE(low_level_sync)
{
   try {
      std::string const host = "127.0.0.1";
      std::string const port = "6379";

      net::io_context ioc;
      net::ip::tcp::resolver resv{ioc};
      auto const res = resv.resolve(host, port);
      net::ip::tcp::socket socket{ioc};
      net::connect(socket, res);

      // Creates the request and writes to the socket.
      request req;
      req.push("HELLO", 3);
      req.push("PING", "Hello world");
      req.push("QUIT");
      redis::detail::write(socket, req);

      std::string buffer;
      result<std::string> resp;

      // Reads the responses to all commands in the request.
      auto dbuffer = net::dynamic_buffer(buffer);
      redis::detail::read(socket, dbuffer);
      redis::detail::read(socket, dbuffer, adapt2(resp));
      redis::detail::read(socket, dbuffer);

      std::cout << "Ping: " << resp.value() << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}
