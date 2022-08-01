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
using connection = aedis::connection<>;

int main()
{
   try {
      net::io_context ioc{1};
      connection conn{ioc};

      std::thread thread{[&]() { ioc.run(); }};

      // Calls async_run in the correct executor.
      net::dispatch(net::bind_executor(ioc, [&]() {
         conn.async_run(net::detached);
      }));

      request req;
      req.push("HELLO", 3);
      req.push("PING");
      req.push("QUIT");

      // Executes commands synchronously.
      std::tuple<aedis::ignore, std::string, aedis::ignore> resp;
      exec(conn, req, adapt(resp));
      thread.join();

      std::cout << "Response: " << std::get<1>(resp) << std::endl;
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
