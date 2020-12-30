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

int main()
{
   try {
      resp::request p;
      p.set("Password", {"12345"});
      p.quit();

      io_context ioc {1};
      tcp::resolver resv(ioc);
      tcp::socket socket {ioc};
      net::connect(socket, resv.resolve("127.0.0.1", "6379"));
      net::write(socket, buffer(p.payload));

      std::string buffer;
      for (;;) {
	 resp::response_simple_string res;
	 resp::read(socket, buffer, res);
	 std::cout << res.result << std::endl;
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

