/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <boost/asio.hpp>
#include <aedis.hpp>
#include <aedis/experimental/sync.hpp>

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using aedis::adapt;
using aedis::resp3::request;
using aedis::experimental::exec;
using aedis::experimental::receive;
using connection = aedis::connection<>;
using node = aedis::resp3::node<std::string>;

// See subscriber.cpp for more info about how to run this example.

int main()
{
   try {
      net::io_context ioc{1};
      connection conn{ioc};

      std::thread thread{[&]() {
         request req;
         req.push("HELLO", 3);
         req.push("SUBSCRIBE", "channel");
         conn.async_run(req, adapt(), net::detached);
         ioc.run();
      }};

      for (std::vector<node> resp;;) {
         boost::system::error_code ec;
         receive(conn, adapt(resp), ec);

         std::cout
            << "Event: "   << resp.at(1).value << "\n"
            << "Channel: " << resp.at(2).value << "\n"
            << "Message: " << resp.at(3).value << "\n"
            << std::endl;

         resp.clear();
      }

      thread.join();

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
