/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>
#include <aedis/utils.hpp>

using namespace aedis;

int main()
{
   try {
      request req;
      req.hello();
      req.rpush("list", {1, 2, 3});
      req.lrange("list");
      req.quit();

      net::io_context ioc {1};
      net::ip::tcp::resolver resv(ioc);
      net::ip::tcp::socket socket {ioc};
      net::connect(socket, resv.resolve("127.0.0.1", "6379"));
      write(socket, req);

      std::string buffer;

      resp::response_ignore hello;
      read(socket, buffer, hello);

      resp::response_number list_size;
      read(socket, buffer, list_size);
      std::cout << list_size.result << std::endl;

      resp::response_basic_array<int> list;
      read(socket, buffer, list);
      print(list.result);

      resp::response_simple_string ok;
      read(socket, buffer, ok);
      std::cout << ok.result << std::endl;

      resp::response_ignore noop;
      read(socket, buffer, noop);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
