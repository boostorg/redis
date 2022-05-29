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
namespace generic = aedis::generic;

using aedis::redis::command;
using aedis::generic::request;
using connection = generic::connection<command>;

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
   db.async_exec(req, generic::adapt(resp), handler);
   db.async_run(handler);
   ioc.run();

   std::cout << std::get<0>(resp) << std::endl;
   std::cout << std::get<1>(resp) << std::endl;
}
