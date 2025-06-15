/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_LOGGER_HPP
#define BOOST_REDIS_LOGGER_HPP

#include <boost/redis/response.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/core/span.hpp>

#include <cstddef>
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
   logger(level l = level::debug, std::function<void(std::string_view)> fn = {})
   : lvl{l}
   , fn{std::move(fn)}
   { }

   level lvl;
   std::function<void(std::string_view)> fn;
};

namespace detail {

// Creates the default logging function. To be used if logger::fn is not specified
std::function<void(std::string_view)> make_clog_function(std::string prefix);

// Logging functions
void log_resolve(
   const logger& l,
   system::error_code const& ec,
   asio::ip::tcp::resolver::results_type const& res);
void log_connect(const logger& l, system::error_code const& ec, asio::ip::tcp::endpoint const& ep);
void log_ssl_handshake(const logger& l, system::error_code const& ec);
void log_write(const logger& l, system::error_code const& ec, std::string_view payload);
void log_read(const logger& l, system::error_code const& ec, std::size_t n);
void log_hello(const logger& l, system::error_code const& ec, generic_response const& resp);
void trace(const logger& l, std::string_view message);
void trace(const logger& l, std::string_view op, system::error_code const& ec);

}  // namespace detail

}  // namespace boost::redis

#endif  // BOOST_REDIS_LOGGER_HPP
