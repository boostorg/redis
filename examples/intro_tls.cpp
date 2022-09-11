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
using connection = aedis::ssl::connection<boost::asio::ssl::stream<net::ip::tcp::socket>>;

bool verify_certificate(bool, net::ssl::verify_context&)
{
   std::cout << "set_verify_callback" << std::endl;
   return true;
}

auto main() -> int
{
   try {
      net::io_context ioc;

      net::ssl::context ctx{net::ssl::context::sslv23};
      ctx.load_verify_file("ca.pem");

      connection db{ioc, ctx};
      db.next_layer().set_verify_mode(net::ssl::verify_peer);
      db.next_layer().set_verify_callback(verify_certificate);

      request req;
      req.push("PING");
      req.push("QUIT");

      std::tuple<std::string, aedis::ignore> resp;
      endpoint ep{"127.0.0.1", "6379"};
      db.async_run(ep, req, adapt(resp), [&](auto ec, auto) {
         std::cout << ec.message() << std::endl;
         db.close();
      });

      ioc.run();

      std::cout << "Response: " << std::get<0>(resp) << std::endl;
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}
