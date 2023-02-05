/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#include <boost/redis.hpp>
#include <boost/redis/src.hpp>

namespace net = boost::asio;
namespace redis = boost::redis;
using redis::operation;
using redis::request;
using redis::response;
using redis::ignore_t;

void log(boost::system::error_code const& ec, char const* prefix)
{
   std::clog << prefix << ec.message() << std::endl;
}

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
      req.push("QUIT");

      // The response.
      response<ignore_t, std::string, ignore_t> resp;

      net::io_context ioc;

      // IO objects.
      net::ip::tcp::resolver resv{ioc};
      redis::connection conn{ioc};

      // Resolve endpoints.
      net::ip::tcp::resolver::results_type endpoints;

      // async_run callback.
      auto on_run = [](auto ec)
      {
         if (ec)
            return log(ec, "on_run: ");
      };

      // async_exec callback.
      auto on_exec = [&](auto ec, auto)
      {
         if (ec) {
            conn.cancel(operation::run);
            return log(ec, "on_exec: ");
         }

         std::cout << "PING: " << std::get<1>(resp).value() << std::endl;
      };

      // Connect callback.
      auto on_connect = [&](auto ec, auto)
      {
         if (ec)
            return log(ec, "on_connect: ");

         conn.async_run(on_run);
         conn.async_exec(req, resp, on_exec);
      };

      // Resolve callback.
      auto on_resolve = [&](auto ec, auto const& addrs)
      {
         if (ec)
            return log(ec, "on_resolve: ");

         endpoints = addrs;
         net::async_connect(conn.next_layer(), endpoints, on_connect);
      };

      resv.async_resolve(host, port, on_resolve);

      ioc.run();
      return 0;

   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }

   return 1;
}

