/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONFIG_HPP
#define BOOST_REDIS_CONFIG_HPP

#include <boost/redis/request.hpp>

#include <chrono>
#include <limits>
#include <optional>
#include <string>

namespace boost::redis {
namespace detail {

inline request make_hello_request()
{
   request req;
   req.push("HELLO", "3");
   return req;
}

}  // namespace detail

/// Address of a Redis server.
struct address {
   /// Redis host.
   std::string host = "127.0.0.1";
   /// Redis port.
   std::string port = "6379";
};

/// Configure parameters used by the connection classes.
struct config {
   /// Uses SSL instead of a plain connection.
   bool use_ssl = false;

   /// For TCP connections, hostname and port of the Redis server.
   address addr = address{"127.0.0.1", "6379"};

   /**
    * @brief The UNIX domain socket path where the server is listening.
    *
    * If non-empty, communication with the server will happen using
    * UNIX domain sockets, and @ref addr will be ignored.
    * UNIX domain sockets can't be used with SSL: if `unix_socket` is non-empty,
    * @ref use_ssl must be `false`.
    */
   std::string unix_socket;

   /** @brief Username used for authentication during connection establishment.
    * 
    * During connection establishment, authentication is performed by sending
    * either a `HELLO` (by default) or an `AUTH` command (if @ref use_hello is set to false).
    * This field contains the username to employ.
    *
    * If the username equals the literal `"default"` (the default)
    * and no password is specified, no authentication is performed.
    */
   std::string username = "default";

   /** @brief Password used for authentication during connection establishment.
    * 
    * During connection establishment, authentication is performed by sending
    * either a `HELLO` (by default) or an `AUTH` command (if @ref use_hello is set to false).
    * This field contains the password to employ.
    *
    * If the username equals the literal `"default"` (the default)
    * and no password is specified, no authentication is performed.
    */
   std::string password;

   /** @brief Client name parameter to use during connection establishment.
    *
    * During connection establishment, the client's name is set by sending
    * either a `HELLO` (by default) or a `CLIENT SETNAME` command
    * (if @ref use_hello is set to false).
    * This field contains the name to use.
    *
    * Set this field to the empty string to disable setting the client's name
    * automatically during connection establishment.
    */
   std::string clientname = "Boost.Redis";

   /** @brief Database index to pass to the `SELECT` command during connection establishment.
    * 
    * If this field is set to a non-empty optional, and its value is different than zero,
    * a `SELECT` command will be issued during connection establishment to set the logical
    * database index. By default, no `SELECT` command is sent.
    */
   std::optional<int> database_index = 0;

   /// Message used by the health-checker in @ref boost::redis::basic_connection::async_run.
   std::string health_check_id = "Boost.Redis";

   /**
    * @brief (Deprecated) Sets the logger prefix, a string printed before log messages.
    * 
    * Setting a prefix in this struct is deprecated. If you need to change how log messages
    * look like, please construct a logger object passing a formatting function, and use that
    * logger in connection's constructor. This member will be removed in subsequent releases.
    */
   std::string log_prefix = "(Boost.Redis) ";

   /// Time span that the resolve operation is allowed to elapse.
   std::chrono::steady_clock::duration resolve_timeout = std::chrono::seconds{10};

   /// Time span that the connect operation is allowed to elapse.
   std::chrono::steady_clock::duration connect_timeout = std::chrono::seconds{10};

   /// Time span that the SSL handshake operation is allowed to elapse.
   std::chrono::steady_clock::duration ssl_handshake_timeout = std::chrono::seconds{10};

   /** @brief Time span between successive health checks.
    *  Set to zero to disable health-checks pass zero as duration.
    */
   std::chrono::steady_clock::duration health_check_interval = std::chrono::seconds{2};

   /** @brief Time span to wait between successive connection retries.
    *  Set to zero to disable reconnection.
    */
   std::chrono::steady_clock::duration reconnect_wait_interval = std::chrono::seconds{1};

   /** @brief Maximum size of the socket read-buffer in bytes.
    *  
    *  Sets a limit on how much data is allowed to be read into the
    *  read buffer. It can be used to prevent DDOS.
    */
   std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)();

   /** @brief Grow size of the read buffer.
    *
    * The size by which the read buffer grows when more space is
    * needed. This can help avoiding some memory allocations. Once the
    * maximum size is reached no more memory allocations are made
    * since the buffer is reused.
    */
   std::size_t read_buffer_append_size = 4096;

   /** @brief Whether to use the `HELLO` command during connection establishment or not
    * 
    * By default, a `HELLO` command is used during connection establishment to:
    *
    *  @li Upgrade the protocol to RESP3, which is more feature-rich.
    *  @li Perform authentication (see @ref username and @ref password).
    *  @li Set the client's name (see @ref clientname).
    *
    * Some RESP-compatible databases do not support the `HELLO` command properly.
    * To interact with these databases, set this field to false to avoid using the
    * `HELLO` command. These system *must speak the RESP3 protocol by default*.
    * Automatic authentication and client name setting will still be performed
    * with the same semantics, but using different commands.
    */
   bool use_setup = false;

   request setup = detail::make_hello_request();
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_CONFIG_HPP
