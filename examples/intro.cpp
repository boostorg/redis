/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <memory>
#include <vector>
#include <cstdio>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::resp3::node;
using aedis::adapter::adapt;
using aedis::redis::command;
using aedis::generic::request;
using connection = aedis::generic::connection<command>;

auto run_handler =[](auto ec)
{
   std::printf("Run: %s\n", ec.message().data());
};

auto exec_handler = [](auto ec, std::size_t read_size)
{
   std::printf("Exec: %s %lu\n", ec.message().data(), read_size);
};

int main()
{
   std::vector<node<std::string>> resp;

   net::io_context ioc;
   connection db{ioc.get_executor(), adapt(resp)};

   request<command> req;
   req.push(command::set, "intro-key", "message1");
   req.push(command::get, "intro-key");
   req.push(command::quit);
   db.async_exec(req, exec_handler);

   db.async_run(run_handler);
   ioc.run();

   for (auto const& e: resp)
      std::cout << e.value << std::endl;
}
