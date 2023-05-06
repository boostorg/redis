/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_LOGGER_HPP
#define BOOST_REDIS_LOGGER_HPP

#include <boost/redis/response.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <string>

namespace boost::system {class error_code;}

namespace boost::redis {

/** @brief Logger class
 *  @ingroup high-level-api
 *
 *  The class can be passed to the connection objects to log to `std::clog`
 */
class logger {
public:
   /** @brief Syslog-like log levels
    *  @ingroup high-level-api
    */
   enum class level
   {  /// Emergency
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
      debug
   };

   /** @brief Constructor
    *  @ingroup high-level-api
    *
    *  @param l Log level.
    */
   logger(level l = level::info)
   : level_{l}
   {}

   /** @brief Called when the resolve operation completes.
    *  @ingroup high-level-api
    *
    *  @param ec Error returned by the resolve operation.
    *  @param res Resolve results.
    */
   void on_resolve(system::error_code const& ec, asio::ip::tcp::resolver::results_type const& res);

   /** @brief Called when the connect operation completes.
    *  @ingroup high-level-api
    *
    *  @param ec Error returned by the connect operation.
    *  @param ep Endpoint to which the connection connected.
    */
   void on_connect(system::error_code const& ec, asio::ip::tcp::endpoint const& ep);

   /** @brief Called when the ssl handshake operation completes.
    *  @ingroup high-level-api
    *
    *  @param ec Error returned by the handshake operation.
    */
   void on_ssl_handshake(system::error_code const& ec);

   /** @brief Called when the connection is lost.
    *  @ingroup high-level-api
    *
    *  @param ec Error returned when the connection is lost.
    */
   void on_connection_lost(system::error_code const& ec);

   /** @brief Called when the write operation completes.
    *  @ingroup high-level-api
    *
    *  @param ec Error code returned by the write operation.
    *  @param payload The payload written to the socket.
    */
   void on_write(system::error_code const& ec, std::string const& payload);

   /** @brief Called when the `HELLO` request completes.
    *  @ingroup high-level-api
    *
    *  @param ec Error code returned by the async_exec operation.
    *  @param resp Response sent by the Redis server.
    */
   void on_hello(system::error_code const& ec, generic_response const& resp);

   /** @brief Sets a prefix to every log message
    *  @ingroup high-level-api
    *
    *  @param prefix The prefix.
    */
   void set_prefix(std::string_view prefix)
   {
      prefix_ = prefix;
   }

private:
   void write_prefix();
   level level_;
   std::string_view prefix_;
};

} // boost::redis

#endif // BOOST_REDIS_LOGGER_HPP
