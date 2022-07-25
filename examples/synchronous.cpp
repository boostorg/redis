/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <boost/asio.hpp>
#include <aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;

using aedis::adapt;
using aedis::resp3::request;
using connection = aedis::connection<>;

auto handler = [](auto ec)
   { std::cout << ec.message() << std::endl; };

int main()
{
   try {
      request req;
      req.push("HELLO", 3);
      req.push("PING");
      req.push("QUIT");

      std::tuple<aedis::ignore, std::string, aedis::ignore> resp;

      net::io_context ioc;
      auto db = std::make_shared<connection>(ioc);

      std::thread thread([&ioc, db](){
         db->async_run("127.0.0.1", "6379", handler);
         ioc.run();
      });

      auto fut = db->async_exec(req, adapt(resp), net::use_future);

      // Do other things here while the command executes.

      std::cout
         << "Future result: " << fut.get() << "\n" // Blocks
         << "Response: " << std::get<1>(resp)
         << std::endl;

      ioc.stop();
      thread.join();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
