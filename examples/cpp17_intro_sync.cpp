/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <thread>
#include <iostream>
#include <boost/asio.hpp>
#include <aedis.hpp>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
namespace resp3 = aedis::resp3;
using aedis::adapt;
using connection = aedis::connection;

template <class Adapter>
auto exec(std::shared_ptr<connection> conn, resp3::request const& req, Adapter adapter)
{
   net::dispatch(
      conn->get_executor(),
      net::deferred([&]() { return conn->async_exec(req, adapter, net::deferred); }))
      (net::use_future).get();
}

auto logger = [](auto const& ec)
   { std::clog << "Run: " << ec.message() << std::endl; };

auto main(int argc, char * argv[]) -> int
{
   try {
      std::string host = "127.0.0.1";
      std::string port = "6379";

      if (argc == 3) {
         host = argv[1];
         port = argv[2];
      }

      net::io_context ioc{1};

      auto conn = std::make_shared<connection>(ioc);
      net::ip::tcp::resolver resv{ioc};
      auto const res = resv.resolve(host, port);
      net::connect(conn->next_layer(), res);
      std::thread t{[conn, &ioc]() {
         conn->async_run(logger);
         ioc.run();
      }};

      resp3::request req;
      req.push("HELLO", 3);
      req.push("PING");
      req.push("QUIT");

      std::tuple<aedis::ignore, std::string, aedis::ignore> resp;
      exec(conn, req, adapt(resp));

      std::cout << "Response: " << std::get<1>(resp) << std::endl;

      t.join();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
