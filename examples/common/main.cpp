/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace net = boost::asio;
extern net::awaitable<void> async_main();

// The main function used in our examples.
auto main() -> int
{
   try {
      net::io_context ioc;
      net::co_spawn(ioc, async_main(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
   }
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT)
auto main() -> int {std::cout << "Requires coroutine support." << std::endl; return 0;}
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
