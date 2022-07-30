/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <boost/asio.hpp>
#include <aedis.hpp>
#include <aedis/experimental/sync.hpp>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

using aedis::adapt;
using aedis::resp3::request;
using aedis::experimental::failover_config;
using connection = aedis::connection<>;

int main()
{
   try {
      aedis::experimental::sync<connection> conn{failover_config{"127.0.0.1", "6379"}};

      request req;
      req.push("HELLO", 3);
      req.push("PING");
      req.push("QUIT");
      std::tuple<aedis::ignore, std::string, aedis::ignore> resp;
      conn.exec(req, adapt(resp));

      std::cout << "Response: " << std::get<1>(resp) << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
