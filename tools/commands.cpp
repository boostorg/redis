/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <algorithm>
#include <cctype>

#include <aedis/aedis.hpp>

using aedis::command;
using aedis::resp3::serializer;
using aedis::resp3::read;
using aedis::resp3::adapt;
using aedis::resp3::node;

namespace net = aedis::net;
using net::ip::tcp;
using net::write;
using net::buffer;

std::string toupper(std::string s)
{
   std::transform(std::begin(s), std::end(s), std::begin(s),
                  [](unsigned char c){ return std::toupper(c); });
   return s;
}

std::vector<std::string>
get_cmd_names(std::vector<node> const& resp)
{
   if (std::empty(resp)) {
      std::cerr << "Response is empty." << std::endl;
      return {};
   }

   std::vector<std::string> ret;
   for (auto i = 0ULL; i < std::size(resp); ++i) {
      if (resp.at(i).depth == 1)
         ret.push_back(resp.at(i + 1).data);
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

   std::cout << "   unknown\n};\n";
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
     connect(socket, res);

     serializer<command> sr;
     sr.push(command::hello, 3);
     sr.push(command::command);
     sr.push(command::quit);
     write(socket, buffer(sr.request()));

     std::vector<node> resp;

     std::string buffer;
     read(socket, buffer);
     read(socket, buffer, adapt(resp));
     read(socket, buffer);

     auto const cmds = get_cmd_names(resp);
     print_cmds_enum(cmds);
     std::cout << "\n";
     print_cmds_strs(cmds);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

