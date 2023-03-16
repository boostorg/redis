/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include "start.hpp"
#include <boost/redis/address.hpp>
#include <boost/asio/awaitable.hpp>
#include <string>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

extern boost::asio::awaitable<void> co_main(boost::redis::address const&);

auto main(int argc, char * argv[]) -> int
{
   boost::redis::address addr;

   if (argc == 3) {
      addr.host = argv[1];
      addr.port = argv[2];
   }

   return start(co_main(addr));
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)

#include <iostream>

auto main() -> int
{
   std::cout << "Requires coroutine support." << std::endl;
   return 0;
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
