/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONFIG_HPP
#define BOOST_REDIS_CONFIG_HPP

#include <boost/config.hpp>

#include <chrono>
#include <limits>
#include <optional>
#include <string>

namespace boost::redis {

/** @brief Address of a Redis server
 *  @ingroup high-level-api
 */
struct address {
   /// Redis host.
   std::string host = "127.0.0.1";
   /// Redis port.
   std::string port = "6379";
};

/** @brief Configure parameters used by the connection classes
 *  @ingroup high-level-api
 */
struct config {
   /// Uses SSL instead of a plain connection.
   bool use_ssl = false;

   /// Address of the Redis server.
   address addr = address{"127.0.0.1", "6379"};

   /** @brief Username passed to the
    * [HELLO](https://redis.io/commands/hello/) command.  If left
    * empty `HELLO` will be sent without authentication parameters.
    */
   std::string username = "default";

   /** @brief Password passed to the
    * [HELLO](https://redis.io/commands/hello/) command.  If left
    * empty `HELLO` will be sent without authentication parameters.
    */
   std::string password;

   /// Client name parameter of the [HELLO](https://redis.io/commands/hello/) command.
   std::string clientname = "Boost.Redis";

   /// Database that will be passed to the [SELECT](https://redis.io/commands/hello/) command.
   std::optional<int> database_index = 0;

   /// Message used by the health-checker in `boost::redis::connection::async_run`.
   std::string health_check_id = "Boost.Redis";

   /// Logger prefix, see `boost::redis::logger`.
   BOOST_DEPRECATED(
      "Setting the logger prefix should be done by creating a logger using "
      "boost::redis::make_clog_logger or constructing a logger object with a user-supplied "
      "function. This member is deprecated and will be removed in subsequent releases.")
   std::string log_prefix = "(Boost.Redis) ";

   /// Time the resolve operation is allowed to last.
   std::chrono::steady_clock::duration resolve_timeout = std::chrono::seconds{10};

   /// Time the connect operation is allowed to last.
   std::chrono::steady_clock::duration connect_timeout = std::chrono::seconds{10};

   /// Time the SSL handshake operation is allowed to last.
   std::chrono::steady_clock::duration ssl_handshake_timeout = std::chrono::seconds{10};

   /** Health checks interval.  
    *  
    *  To disable health-checks pass zero as duration.
    */
   std::chrono::steady_clock::duration health_check_interval = std::chrono::seconds{2};

   /** @brief Time waited before trying a reconnection.
    *  
    *  To disable reconnection pass zero as duration.
    */
   std::chrono::steady_clock::duration reconnect_wait_interval = std::chrono::seconds{1};

   /** @brief Maximum size of a socket read.
    *  
    *  Sets a limit on how much data is allowed to be read into the
    *  read buffer. It can be used to prevent DDOS.
    */
   std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)();
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_CONFIG_HPP
