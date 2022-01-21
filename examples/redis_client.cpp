/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "lib/responses.hpp"
#include "src.hpp"

namespace net = aedis::net;
using aedis::command;
using aedis::resp3::experimental::client;

int main()
{
   try {
      responses resps;

      auto on_msg = [&resps](std::error_code ec, command cmd)
      {
	 if (ec) {
	    std::cerr << "Error: " << ec.message() << std::endl;
	    return;
	 }

	 switch (cmd) {
	    case command::ping:
	    {
	       std::cout << "ping: " << resps.simple_string << std::endl;
	       resps.simple_string.clear();
	    } break;
	    case command::quit:
	    {
	       std::cout << "quit: " << resps.simple_string << std::endl;
	       resps.simple_string.clear();
	    } break;
	    case command::incr:
	    {
	       std::cout << "incr: " << resps.number << std::endl;
	    } break;
	    default: { assert(false); }
	 }
      };

      net::io_context ioc{1};

      auto db = std::make_shared<client>(ioc.get_executor());
      db->set_adapter(adapter_wrapper{resps});
      db->set_msg_callback(on_msg);
      db->send(command::ping, "O rato roeu a roupa do rei de Roma");
      db->send(command::incr, "redis-client-counter");
      db->send(command::quit);
      db->prepare();

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
