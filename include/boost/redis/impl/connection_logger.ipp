/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/connection_logger.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <string>

namespace boost::redis::detail {

inline void format_tcp_endpoint(const asio::ip::tcp::endpoint& ep, std::string& to)
{
   // This formatting is inspired by Asio's endpoint operator<<
   const auto& addr = ep.address();
   if (addr.is_v6())
      to += '[';
   to += addr.to_string();
   if (addr.is_v6())
      to += ']';
   to += ':';
   to += std::to_string(ep.port());
}

inline void format_error_code(system::error_code ec, std::string& to)
{
   // Using error_code::what() includes any source code info
   // that the error may contain, making the messages too long.
   // This implementation was taken from error_code::what()
   to += ec.message();
   to += " [";
   to += ec.to_string();
   to += ']';
}

template <class Fn>
inline void do_log(connection_logger& lgr, logger::level msg_level, Fn fn)
{
   if (lgr.lvl < msg_level)
      return;

   fn()
}

void connection_logger::on_resolve(
   system::error_code const& ec,
   asio::ip::tcp::resolver::results_type const& res)
{
   if (logger_.lvl < logger::level::info)
      return;

   if (ec) {
      msg_ = "Error resolving the server hostname: ";
      format_error_code(ec, msg_);
   } else {
      msg_ = "Resolve results: ";
      auto iter = res.cbegin();
      auto end = res.cend();

      if (iter != end) {
         format_tcp_endpoint(iter->endpoint(), msg_);
         ++iter;
         for (; iter != end; ++iter) {
            msg_ += ", ";
            format_tcp_endpoint(iter->endpoint(), msg_);
         }
      }
   }

   logger_.fn(logger::level::info, msg_);
}

void connection_logger::on_connect(system::error_code const& ec, asio::ip::tcp::endpoint const& ep)
{
   if (logger_.lvl < logger::level::info)
      return;

   if (ec) {
      msg_ = "Failed connecting to the server: ";
      format_error_code(ec, msg_);
   } else {
      msg_ = "Connected to ";
      format_tcp_endpoint(ep, msg_);
   }

   logger_.fn(logger::level::info, msg_);
}

void connection_logger::on_ssl_handshake(system::error_code const& ec)
{
   if (logger_.lvl < logger::level::info)
      return;

   msg_ = "SSL handshake: ";
   format_error_code(ec, msg_);

   logger_.fn(logger::level::info, msg_);
}

void connection_logger::on_write(system::error_code const& ec, std::size_t n)
{
   if (logger_.lvl < logger::level::info)
      return;

   msg_ = "writer_op: ";
   if (ec) {
      format_error_code(ec, msg_);
   } else {
      msg_ += std::to_string(n);
      msg_ += " bytes written.";
   }

   logger_.fn(logger::level::info, msg_);
}

void connection_logger::on_read(system::error_code const& ec, std::size_t n)
{
   if (logger_.lvl < logger::level::info)
      return;

   msg_ = "reader_op: ";
   if (ec) {
      format_error_code(ec, msg_);
   } else {
      msg_ += std::to_string(n);
      msg_ += " bytes read.";
   }

   logger_.fn(logger::level::info, msg_);
}

void connection_logger::on_hello(system::error_code const& ec, generic_response const& resp)
{
   if (logger_.lvl < logger::level::info)
      return;

   msg_ = "hello_op: ";
   if (ec) {
      format_error_code(ec, msg_);
      if (resp.has_error()) {
         msg_ += " (";
         msg_ += resp.error().diagnostic;
         msg_ += ')';
      }
   } else {
      msg_ += "success";
   }

   logger_.fn(logger::level::info, msg_);
}

void connection_logger::trace(std::string_view message)
{
   if (logger_.lvl < logger::level::debug)
      return;
   logger_.fn(logger::level::debug, message);
}

void connection_logger::trace(std::string_view op, system::error_code const& ec)
{
   if (logger_.lvl < logger::level::debug)
      return;

   msg_ = op;
   msg_ += ": ";
   format_error_code(ec, msg_);

   logger_.fn(logger::level::debug, msg_);
}

}  // namespace boost::redis::detail