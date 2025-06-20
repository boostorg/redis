/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/logger.hpp>

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>

// TODO: test edge cases here
boost::redis::logger boost::redis::make_stderr_logger(logger::level lvl, std::string prefix)
{
   return logger(lvl, [prefix = std::move(prefix)](logger::level, std::string_view msg) {
      // If the message is empty, data() might return a null pointer
      const char* msg_ptr = msg.empty() ? "" : msg.data();

      // Precision should be an int when passed to fprintf. Technically,
      // message could be larger than INT_MAX. Impose a sane limit on message sizes
      // to prevent memory problems
      int precision = (std::min)(msg.size(), static_cast<std::size_t>(0xffffu));

      // Log the message. None of our messages should contain NULL bytes, so this should be OK.
      // We choose fprintf over std::clog because it's safe in multi-threaded environments.
      std::fprintf(stderr, "%s%.*s\n", prefix.c_str(), precision, msg_ptr);
   });
}
