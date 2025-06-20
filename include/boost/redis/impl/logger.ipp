/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/logger.hpp>

#include <iostream>
#include <string>
#include <string_view>

boost::redis::logger boost::redis::make_clog_logger(logger::level lvl, std::string prefix)
{
   return logger(lvl, [prefix = std::move(prefix)](logger::level, std::string_view msg_) {
      std::clog << prefix << msg_ << std::endl;
   });
}
