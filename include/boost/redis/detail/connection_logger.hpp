/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONNECTION_LOGGER_HPP
#define BOOST_REDIS_CONNECTION_LOGGER_HPP

#include <boost/redis/logger.hpp>

#include <boost/system/error_code.hpp>

namespace boost::redis::detail {

// Wraps a logger and a string buffer for re-use, and provides
// utility functions to format the log messages that we use.
// The long-term trend will be moving most of this class to finite state
// machines as we write them
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
   void on_connect(system::error_code const& ec, std::string_view unix_socket_ep);
   void log_error(std::string_view op, system::error_code const& ec);
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_LOGGER_HPP
