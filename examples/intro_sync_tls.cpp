/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <aedis.hpp>
#include <aedis/ssl/sync.hpp>
#include <aedis/ssl/connection.hpp>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using aedis::endpoint;
using connection = aedis::ssl::sync<aedis::ssl::connection<net::ssl::stream<net::ip::tcp::socket>>>;

bool verify_certificate(bool, net::ssl::verify_context&)
{
   std::cout << "set_verify_callback" << std::endl;
   return true;
}

int main()
{
   try {
      net::io_context ioc{1};

      net::ssl::context ctx{net::ssl::context::sslv23};

      auto work = net::make_work_guard(ioc);
      std::thread t1{[&]() { ioc.run(); }};

      connection conn{work.get_executor(), ctx};
      conn.next_layer().next_layer().set_verify_mode(net::ssl::verify_peer);
      conn.next_layer().next_layer().set_verify_callback(verify_certificate);

      std::thread t2{[&]() {
         boost::system::error_code ec;
         conn.run({"127.0.0.1", "6379"}, ec);
      }};

      request req;
      req.push("PING");
      req.push("QUIT");

      std::tuple<std::string, aedis::ignore> resp;
      conn.exec(req, adapt(resp));
      std::cout << "Response: " << std::get<0>(resp) << std::endl;

      work.reset();

      t1.join();
      t2.join();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
