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
using client_type = aedis::generic::client<net::ip::tcp::socket, command>;

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

   client_type db{ioc.get_executor()};
   db.set_adapter(adapt(resp));

   request<command> req2;
   req2.push(command::set, "intro-key", "message1");
   req2.push(command::get, "intro-key");
   db.async_exec(req2, exec_handler);

   request<command> req3;
   req3.push(command::set, "intro-key", "message2");
   req3.push(command::get, "intro-key");
   db.async_exec(req3, exec_handler);

   request<command> req4;
   req4.push(command::quit);
   db.async_exec(req4, exec_handler);

   db.async_run(run_handler);
   ioc.run();

   for (auto const& e: resp)
      std::cout << e.value << std::endl;
}
