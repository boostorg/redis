/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_LOGGER_HPP
#define BOOST_REDIS_LOGGER_HPP

#include <boost/redis/response.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <functional>
#include <string>
#include <string_view>

namespace boost::redis {

/** @brief Logger class
 *  @ingroup high-level-api
 *
 *  The class can be passed to the connection objects to log to `std::clog`
 *
 *  Notice that currently this class has no stable interface. Users
 *  that don't want any logging can disable it by contructing a logger
 *  with logger::level::emerg to the connection.
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
   logger(level l = level::info, std::function<void(level, std::string_view)> fn = {})
   : lvl{l}
   , fn{std::move(fn)}
   { }

   level lvl;
   std::function<void(level, std::string_view)> fn;
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_LOGGER_HPP
