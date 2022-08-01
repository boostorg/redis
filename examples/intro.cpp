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
   net::io_context ioc;
   connection db{ioc};

   request req;
   req.push("HELLO", 3);
   req.push("PING");
   req.push("QUIT");

   std::tuple<aedis::ignore, std::string, aedis::ignore> resp;
   db.async_run(req, adapt(resp), [](auto ec, auto) {
      std::cout << ec.message() << std::endl;
   });

   ioc.run();

   std::cout << std::get<1>(resp) << std::endl;
}
