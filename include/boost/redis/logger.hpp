/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_LOGGER_HPP
#define BOOST_REDIS_LOGGER_HPP

#include <boost/redis/response.hpp>

#include <boost/asio/ip/tcp.hpp>

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
   logger(level l = level::debug, std::function<void(level, std::string_view)> fn = {})
   : lvl{l}
   , fn{std::move(fn)}
   { }

   level lvl;
   std::function<void(level, std::string_view)> fn;
};

/// Creates a logger that logs messages to std::clog, prefixed by prefix.
/// Ignores messages with level less than lvl.
logger make_clog_logger(logger::level lvl, std::string prefix);

namespace detail {

// Wraps a logger and a string buffer for re-use, and provides
// utility functions to format the log messages that we use
class connection_logger {
   logger logger_;
   std::string msg_;

public:
   connection_logger() = default;

   void reset(logger&& logger) { logger_ = std::move(logger); }

   void on_resolve(system::error_code const& ec, asio::ip::tcp::resolver::results_type const& res);
   void on_connect(system::error_code const& ec, asio::ip::tcp::endpoint const& ep);
   void on_ssl_handshake(system::error_code const& ec);
   void on_write(system::error_code const& ec, std::size_t n);
   void on_read(system::error_code const& ec, std::size_t n);
   void on_hello(system::error_code const& ec, generic_response const& resp);
   void trace(std::string_view message);
   void trace(std::string_view op, system::error_code const& ec);
};

}  // namespace detail

}  // namespace boost::redis

#endif  // BOOST_REDIS_LOGGER_HPP
