/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>
#include <aedis/utils.hpp>

using namespace aedis;

enum class events {ignore};

int main()
{
   try {
      request<events> req;
      req.hello();
      req.set("Password", {"12345"});
      req.get("Password");
      req.quit();

      net::io_context ioc {1};
      net::ip::tcp::resolver resv(ioc);
      net::ip::tcp::socket socket {ioc};
      net::connect(socket, resv.resolve("127.0.0.1", "6379"));
      write(socket, req);

      std::string buffer;
      resp::response_map hello;
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

