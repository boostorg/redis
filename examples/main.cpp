/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include "start.hpp"
#include <boost/asio/awaitable.hpp>
#include <string>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

extern boost::asio::awaitable<void> co_main(std::string, std::string);

auto main(int argc, char * argv[]) -> int
{
   std::string host = "127.0.0.1";
   std::string port = "6379";

   if (argc == 3) {
      host = argv[1];
      port = argv[2];
   }

   return start(co_main(host, port));
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)

#include <iostream>

auto main() -> int
{
   std::cout << "Requires coroutine support." << std::endl;
   return 0;
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
