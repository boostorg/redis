/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include "start.hpp"
#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <iostream>

namespace net = boost::asio;

auto start(net::awaitable<void> op) -> int
{
   try {
      net::io_context ioc;
      net::co_spawn(ioc, std::move(op), [](std::exception_ptr p) {
         if (p)
            std::rethrow_exception(p);
      });
      ioc.run();

      return 0;

   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }

   return 1;
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
