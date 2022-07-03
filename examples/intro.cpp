/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <boost/asio.hpp>
#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;

using aedis::adapt;
using aedis::resp3::request;
using connection = aedis::connection<>;

int main()
{
   request req;
   req.push("PING");
   req.push("QUIT");

   std::tuple<std::string, std::string> resp;

   net::io_context ioc;

   connection db{ioc};
   db.async_exec("127.0.0.1", "6379", req, adapt(resp),
      [](auto ec, auto) { std::cout << ec.message() << std::endl; });

   ioc.run();

   // Print
   std::cout << std::get<0>(resp) << std::endl;
   std::cout << std::get<1>(resp) << std::endl;
}
