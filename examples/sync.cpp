/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

namespace net = aedis::net;

using namespace net;
using namespace aedis;

void sync_example1()
{
   io_context ioc {1};

   tcp::resolver resv(ioc);
   tcp::socket socket {ioc};
   net::connect(socket, resv.resolve("127.0.0.1", "6379"));

   resp::pipeline p;
   p.ping();
   p.quit();

   net::write(socket, buffer(p.payload));

   resp::buffer buffer;
   resp::response_vector<std::string> res;
   resp::read(socket, buffer, res);
   print(res.result);
}

void sync_example2()
{
   io_context ioc {1};

   tcp::resolver resv(ioc);
   tcp::socket socket {ioc};
   net::connect(socket, resv.resolve("127.0.0.1", "6379"));

   resp::pipeline p;
   p.multi();
   p.ping();
   p.set("Name", {"Marcelo"});
   p.incr("Age");
   p.exec();
   p.quit();

   net::write(socket, buffer(p.payload));

   resp::buffer buffer;
   for (;;) {
      boost::system::error_code ec;
      resp::response_vector<std::string> res;
      resp::read(socket, buffer, res, ec);
      if (ec) {
	 std::cerr << ec.message() << std::endl;
	 break;
      }
      print(res.result);
   }
}

int main()
{
   sync_example1();
   sync_example2();
}

