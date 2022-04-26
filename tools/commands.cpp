/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <algorithm>
#include <cctype>

#include <boost/asio/connect.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
namespace resp3 = aedis::resp3;

using aedis::redis::command;
using aedis::generic::make_serializer;
using aedis::resp3::node;
using aedis::adapter::adapt;
using net::ip::tcp;
using net::write;
using net::buffer;
using net::dynamic_buffer;

std::string toupper(std::string s)
{
   std::transform(std::begin(s), std::end(s), std::begin(s),
                  [](unsigned char c){ return std::toupper(c); });
   return s;
}

std::vector<std::string>
get_cmd_names(std::vector<node<std::string>> const& resp)
{
   if (resp.empty()) {
      std::cerr << "Response is empty." << std::endl;
      return {};
   }

   std::vector<std::string> ret;
   for (auto i = 0ULL; i < resp.size(); ++i) {
      if (resp.at(i).depth == 1)
         ret.push_back(resp.at(i + 1).value);
   }

   std::sort(std::begin(ret), std::end(ret));
   return ret;
}

void print_cmds_enum(std::vector<std::string> const& cmds)
{
   std::cout << "enum class command {\n";
   for (auto const& cmd : cmds) {
      std::cout
         << "   /// https://redis.io/commands/" << cmd << "\n"
         << "   " << cmd << ",\n";
   }

   std::cout << "   invalid\n};\n";
}

void print_cmds_strs(std::vector<std::string> const& cmds)
{
   std::cout << "   static char const* table[] = {\n";
   for (auto const& cmd : cmds) {
      std::cout << "      \"" << toupper(cmd) << "\",\n";
   }

   std::cout << "   };\n";
}

int main()
{
   try {
     net::io_context ioc;
     tcp::resolver resv{ioc};
     auto const res = resv.resolve("127.0.0.1", "6379");
     tcp::socket socket{ioc};
     net::connect(socket, res);

     std::string request;
     auto sr = make_serializer(request);
     sr.push(command::hello, 3);
     sr.push(command::command);
     sr.push(command::quit);
     write(socket, buffer(request));

     std::vector<node<std::string>> resp;

     std::string buffer;
     resp3::read(socket, dynamic_buffer(buffer));
     resp3::read(socket, dynamic_buffer(buffer), adapt(resp));
     resp3::read(socket, dynamic_buffer(buffer));

     auto const cmds = get_cmd_names(resp);
     print_cmds_enum(cmds);
     std::cout << "\n";
     print_cmds_strs(cmds);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

