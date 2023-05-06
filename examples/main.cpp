/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include "start.hpp"
#include <boost/redis/config.hpp>
#include <boost/asio/awaitable.hpp>
#include <string>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

using boost::redis::config;

extern boost::asio::awaitable<void> co_main(config const&);

auto main(int argc, char * argv[]) -> int
{
   config cfg;

   if (argc == 3) {
      cfg.addr.host = argv[1];
      cfg.addr.port = argv[2];
   }

   return start(co_main(cfg));
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)

#include <iostream>

auto main() -> int
{
   std::cout << "Requires coroutine support." << std::endl;
   return 0;
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
