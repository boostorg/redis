/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_LOGGER_HPP
#define BOOST_REDIS_LOGGER_HPP

#include <functional>
#include <string_view>

namespace boost::redis {

/** @brief Defines logging configuration
 *  @ingroup high-level-api
 *
 *  See the member descriptions for more info.
 */
struct logger {
   /** @brief Syslog-like log levels
    *  @ingroup high-level-api
    */
   enum class level
   {
      /// Disabled
      disabled,

      /// Emergency
      emerg,

      /// Alert
      alert,

      /// Critical
      crit,

      /// Error
      err,

      /// Warning
      warning,

      /// Notice
      notice,

      /// Info
      info,

      /// Debug
      debug,
   };

   /** @brief Constructor
    *  @ingroup high-level-api
    *
    *  @param l Log level.
    */
   logger(level l, std::function<void(level, std::string_view)> fn)
   : lvl{l}
   , fn{std::move(fn)}
   { }

   logger(level l = level::info);

   /**
    * @brief Defines a severity filter for messages.
    *
    * Only messages with a level >= to the one specified by the logger
    * will be logged.
    */
   level lvl;

   /**
    * @brief Defines a severity filter for messages.
    *
    * Only messages with a level >= to the one specified by the logger
    * will be logged.
    */
   std::function<void(level, std::string_view)> fn;
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_LOGGER_HPP
