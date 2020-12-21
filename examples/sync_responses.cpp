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
      resp::pipeline p;
      p.rpush("list", {1, 2, 3});
      p.lrange("list");
      p.quit();

      io_context ioc {1};
      tcp::resolver resv(ioc);
      tcp::socket socket {ioc};
      net::connect(socket, resv.resolve("127.0.0.1", "6379"));
      net::write(socket, buffer(p.payload));

      std::string buffer;
      resp::response_int<int> list_size;
      resp::read(socket, buffer, list_size);
      std::cout << list_size.result << std::endl;

      resp::response_list<int> list;
      resp::read(socket, buffer, list);
      print(list.result);

      resp::response_string ok;
      resp::read(socket, buffer, ok);
      std::cout << ok.result << std::endl;

      resp::response noop;
      resp::read(socket, buffer, noop);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

