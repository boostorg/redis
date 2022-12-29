/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#include <aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
namespace resp3 = aedis::resp3;
using aedis::resp3::request;
using aedis::adapt;
using aedis::operation;

void log(boost::system::error_code const& ec, char const* prefix)
{
   std::clog << prefix << ec.message() << std::endl;
}

auto main() -> int
{
   try {
      // The request
      resp3::request req;
      req.push("HELLO", 3);
      req.push("PING", "Hello world");
      req.push("QUIT");

      // The response.
      std::tuple<aedis::ignore, std::string, aedis::ignore> resp;

      net::io_context ioc;

      // IO objects.
      net::ip::tcp::resolver resv{ioc};
      aedis::connection conn{ioc};

      // Resolve endpoints.
      net::ip::tcp::resolver::results_type endpoints;

      // async_run callback.
      auto on_run = [](auto ec)
      {
         if (ec)
            return log(ec, "on_exec: ");
      };

      // async_exec callback.
      auto on_exec = [&](auto ec, auto)
      {
         if (ec) {
            conn.cancel(operation::run);
            return log(ec, "on_exec: ");
         }

         std::cout << "PING: " << std::get<1>(resp) << std::endl;
      };

      // Connect callback.
      auto on_connect = [&](auto ec, auto)
      {
         if (ec)
            return log(ec, "on_connect: ");

         conn.async_run(on_run);
         conn.async_exec(req, adapt(resp), on_exec);
      };

      // Resolve callback.
      auto on_resolve = [&](auto ec, auto const& addrs)
      {
         if (ec)
            return log(ec, "on_resolve: ");

         endpoints = addrs;
         net::async_connect(conn.next_layer(), endpoints, on_connect);
      };

      resv.async_resolve("127.0.0.1", "6379", on_resolve);

      ioc.run();
      return 0;

   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }

   return 1;
}

