/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

namespace net = aedis::net;
using namespace aedis;

int main()
{
   try {
      resp::request req;
      req.hello();
      req.set("Password", {"12345"});
      req.get("Password");
      req.quit();

      net::io_context ioc {1};
      tcp::resolver resv(ioc);
      tcp::socket socket {ioc};
      net::connect(socket, resv.resolve("127.0.0.1", "6379"));
      resp::write(socket, req);

      std::string buffer;
      resp::response_flat_map<std::string> hello;
      resp::read(socket, buffer, hello);
      print(hello.result);

      resp::response_simple_string set;
      resp::read(socket, buffer, set);

      resp::response_blob_string get;
      resp::read(socket, buffer, get);
      std::cout << "get: " << get.result << std::endl;

      resp::response_ignore quit;
      resp::read(socket, buffer, quit);
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

