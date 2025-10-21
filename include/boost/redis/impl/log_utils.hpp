/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_LOG_UTILS_HPP
#define BOOST_REDIS_LOG_UTILS_HPP

#include <boost/redis/logger.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace boost::redis::detail {

// Internal trait that defines how to log different types.
// The base template applies to types convertible to string_view
template <class T>
struct log_traits {
   // log should convert the input value to string and append it to the supplied buffer
   static inline void log(std::string& to, std::string_view value) { to += value; }
};

// Formatting size_t and error codes is shared between almost all FSMs, so it's defined here.
// Support for types used only in one FSM should be added in the relevant FSM file.
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

// Logs a message with the specified severity to the logger.
// Formatting won't be performed if the logger's level is inferior to lvl.
// args are stringized using log_traits, and concatenated.
template <class... Args>
void log(buffered_logger& to, logger::level lvl, const Args&... args)
{
   // Severity check
   if (to.lgr.lvl < lvl)
      return;

   // Clear the buffer
   to.buffer.clear();

   // Format all arguments
   auto dummy = {(log_traits<Args>::log(to.buffer, args), 0)...};
   ignore_unused(dummy);

   // Invoke the function
   to.lgr.fn(lvl, to.buffer);
}

// Shorthand for each log level we use
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

template <class... Args>
void log_err(buffered_logger& to, const Args&... args)
{
   log(to, logger::level::err, args...);
}

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_LOGGER_HPP
