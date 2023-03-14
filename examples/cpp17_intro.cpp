/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/redis.hpp>
#include <boost/redis/src.hpp>

namespace net = boost::asio;
using boost::redis::connection;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore_t;
using boost::redis::async_run;
using namespace std::chrono_literals;

auto main(int argc, char * argv[]) -> int
{
   try {
      std::string host = "127.0.0.1";
      std::string port = "6379";

      if (argc == 3) {
         host = argv[1];
         port = argv[2];
      }

      // The request
      request req;
      req.push("HELLO", 3);
      req.push("PING", "Hello world");

      // The response.
      response<ignore_t, std::string> resp;

      net::io_context ioc;
      connection conn{ioc};

      async_run(conn, host, port, 10s, 10s, [&](auto){
         conn.cancel();
      });

      conn.async_exec(req, resp, [&](auto ec, auto){
         if (!ec)
            std::cout << "PING: " << std::get<1>(resp).value() << std::endl;
         conn.cancel();
      });

      ioc.run();
      return 0;

   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
   return 1;
}

