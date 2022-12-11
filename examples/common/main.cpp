/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

#include "common.hpp"

extern boost::asio::awaitable<void> async_main();

auto main() -> int
{
   return run(async_main());
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)

#include <iostream>

auto main() -> int
{
   std::cout << "Requires coroutine support." << std::endl;
   return 0;
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
