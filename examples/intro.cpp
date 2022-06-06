/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <tuple>
#include <boost/asio.hpp>
#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;

using aedis::adapt;
using aedis::command;
using aedis::resp3::request;
using connection = aedis::connection<command>;

auto handler =[](auto ec, auto...)
   { std::cout << ec.message() << std::endl; };

int main()
{
   request<command> req;
   req.push(command::ping, "Ping example");
   req.push(command::quit);

   std::tuple<std::string, std::string> resp;

   net::io_context ioc;
   connection db{ioc};
   db.async_exec(req, adapt(resp), handler);
   db.async_run("127.0.0.1", "6379", handler);
   ioc.run();

   std::cout << std::get<0>(resp) << std::endl;
   std::cout << std::get<1>(resp) << std::endl;
}
