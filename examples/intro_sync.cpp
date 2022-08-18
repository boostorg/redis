/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <boost/asio.hpp>
#include <aedis.hpp>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using connection = aedis::connection<>;

int main()
{
   try {
      net::io_context ioc{1};
      connection conn{ioc};

      std::thread thread{[&]() {
         conn.async_run(net::detached);
         ioc.run();
      }};

      request req;
      req.push("PING");
      req.push("QUIT");

      std::tuple<std::string, aedis::ignore> resp;
      conn.exec(req, adapt(resp));
      thread.join();

      std::cout << "Response: " << std::get<0>(resp) << std::endl;
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
