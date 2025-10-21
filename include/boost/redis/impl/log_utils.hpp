/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_LOG_UTILS_HPP
#define BOOST_REDIS_LOG_UTILS_HPP

#include <boost/redis/logger.hpp>

#include <boost/system/error_code.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace boost::redis::detail {

template <class T>
struct log_traits {
   static inline void log(std::string& to, std::string_view value) { to += value; }
};

template <>
struct log_traits<std::size_t> {
   static inline void log(std::string& to, std::size_t value) { to += std::to_string(value); }
};

template <>
struct log_traits<system::error_code> {
   static inline void log(std::string& to, system::error_code value)
   {
      // Using error_code::what() includes any source code info
      // that the error may contain, making the messages too long.
      // This implementation was taken from error_code::what()
      to += value.message();
      to += " [";
      to += value.to_string();
      to += ']';
   }
};

template <class... Args>
void log(buffered_logger& to, logger::level lvl, const Args&... args)
{
   if (to.lgr.lvl < lvl)
      return;

   to.buffer.clear();

   auto a = {(log_traits<Args>::log(to.buffer, args), 0)...};
   static_cast<void>(a);

   to.lgr.fn(lvl, to.buffer);
}

template <class... Args>
void log_debug(buffered_logger& to, const Args&... args)
{
   log(to, logger::level::debug, args...);
}

template <class... Args>
void log_info(buffered_logger& to, const Args&... args)
{
   log(to, logger::level::info, args...);
}

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_LOGGER_HPP
