/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include <aedis.hpp>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using aedis::endpoint;
using connection = aedis::connection<>;

template <class Adapter>
auto exec(connection& conn, request const& req, Adapter adapter, boost::system::error_code& ec)
{
   net::dispatch(
      conn.get_executor(),
      net::deferred([&]() { return conn.async_exec(req, adapter, net::deferred); }))
      (net::redirect_error(net::use_future, ec)).get();
}

auto logger = [](auto const& ec)
   { std::clog << "Run: " << ec.message() << std::endl; };

int main()
{
   try {
      net::io_context ioc{1};

      connection conn{ioc};
      std::thread t{[&]() {
         conn.async_run({"127.0.0.1", "6379"}, logger);
         ioc.run();
      }};

      request req;
      req.push("PING");
      req.push("QUIT");

      boost::system::error_code ec;
      std::tuple<std::string, aedis::ignore> resp;
      exec(conn, req, adapt(resp), ec);

      std::cout
         << "Exec: " << ec.message() << "\n"
         << "Response: " << std::get<0>(resp) << std::endl;

      t.join();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
