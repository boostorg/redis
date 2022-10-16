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
using connection = aedis::ssl::connection<net::ssl::stream<net::ip::tcp::socket>>;

auto const logger = [](auto ec, auto...)
   { std::cout << ec.message() << std::endl; };

auto verify_certificate(bool, net::ssl::verify_context&) -> bool
{
   std::cout << "set_verify_callback" << std::endl;
   return true;
}

auto main() -> int
{
   try {
      net::io_context ioc;

      net::ssl::context ctx{net::ssl::context::sslv23};

      connection conn{ioc, ctx};
      conn.next_layer().set_verify_mode(net::ssl::verify_peer);
      conn.next_layer().set_verify_callback(verify_certificate);

      request req;
      req.get_config().cancel_if_not_connected = false;
      req.get_config().cancel_on_connection_lost = true;
      req.push("PING");
      req.push("QUIT");

      std::tuple<std::string, aedis::ignore> resp;
      conn.async_exec(req, adapt(resp), logger);
      conn.async_run({"127.0.0.1", "6379"}, {}, logger);

      ioc.run();

      std::cout << "Response: " << std::get<0>(resp) << std::endl;
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }
}
