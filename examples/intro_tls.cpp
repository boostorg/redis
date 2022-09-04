/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <aedis.hpp>
#include <aedis/ssl/connection.hpp>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;

using aedis::adapt;
using aedis::resp3::request;
using aedis::endpoint;
using connection = aedis::ssl::connection<net::ip::tcp::socket>;

auto main() -> int
{
   try {
      net::io_context ioc;
      net::ssl::context ctx(net::ssl::context::sslv23);
      ctx.load_verify_file("ca.pem");

      connection db{ioc};

      request req;
      req.push("PING");
      req.push("QUIT");

      std::tuple<std::string, aedis::ignore> resp;
      endpoint ep{"127.0.0.1", "6379"};
      db.async_run(ep, req, adapt(resp), [](auto ec, auto) {
         std::cout << ec.message() << std::endl;
      });

      ioc.run();

      std::cout << std::get<0>(resp) << std::endl;
   } catch (...) {
      std::cerr << "Error" << std::endl;
   }
}
