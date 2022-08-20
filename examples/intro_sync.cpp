/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <boost/asio/io_context.hpp>
#include <aedis.hpp>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using connection = aedis::sync<aedis::connection<>>;

int main()
{
   try {
      net::io_context ioc{1};
      auto work = net::make_work_guard(ioc);
      std::thread t1{[&]() { ioc.run(); }};

      connection conn{work.get_executor()};
      std::thread t2{[&]() { boost::system::error_code ec; conn.run(ec); }};

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
