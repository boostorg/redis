/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/logger.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <iostream>
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

}  // namespace boost::redis::detail

boost::redis::logger boost::redis::make_clog_logger(logger::level lvl, std::string prefix)
{
   return logger(lvl, [prefix = std::move(prefix)](std::string_view msg) {
      std::clog << prefix << msg << std::endl;
   });
}

void boost::redis::detail::log_resolve(
   const logger& l,
   system::error_code const& ec,
   asio::ip::tcp::resolver::results_type const& res)
{
   if (l.lvl < logger::level::info)
      return;

   // TODO: can we make this non-allocating?
   std::string msg;

   if (ec) {
      msg += "Error resolving the server hostname: ";
      format_error_code(ec, msg);
   } else {
      msg += "Resolve results: ";
      auto iter = res.cbegin();
      auto end = res.cend();

      if (iter != end) {
         format_tcp_endpoint(iter->endpoint(), msg);
         ++iter;
         for (; iter != end; ++iter) {
            msg += ", ";
            format_tcp_endpoint(iter->endpoint(), msg);
         }
      }
   }

   l.fn(msg);
}

void boost::redis::detail::log_connect(
   const logger& l,
   system::error_code const& ec,
   asio::ip::tcp::endpoint const& ep)
{
   if (l.lvl < logger::level::info)
      return;

   std::string msg;

   if (ec) {
      msg += "Failed connecting to the server: ";
      format_error_code(ec, msg);
   } else {
      msg += "Connected to ";
      format_tcp_endpoint(ep, msg);
   }

   l.fn(msg);
}

void boost::redis::detail::log_ssl_handshake(const logger& l, system::error_code const& ec)
{
   if (l.lvl < logger::level::info)
      return;

   std::string msg{"SSL handshake: "};
   format_error_code(ec, msg);

   l.fn(msg);
}

void boost::redis::detail::log_write(
   const logger& l,
   system::error_code const& ec,
   std::string_view payload)
{
   if (l.lvl < logger::level::info)
      return;

   std::string msg{"writer_op: "};
   if (ec) {
      format_error_code(ec, msg);
   } else {
      msg += std::to_string(payload.size());
      msg += " bytes written.";
   }

   l.fn(msg);
}

void boost::redis::detail::log_read(const logger& l, system::error_code const& ec, std::size_t n)
{
   if (l.lvl < logger::level::info)
      return;

   std::string msg{"reader_op: "};
   if (ec) {
      format_error_code(ec, msg);
   } else {
      msg += std::to_string(n);
      msg += " bytes read.";
   }

   l.fn(msg);
}

void boost::redis::detail::log_hello(
   const logger& l,
   system::error_code const& ec,
   generic_response const& resp)
{
   if (l.lvl < logger::level::info)
      return;

   std::string msg{"hello_op: "};
   if (ec) {
      format_error_code(ec, msg);
      if (resp.has_error()) {
         msg += " (";
         msg += resp.error().diagnostic;
         msg += ')';
      }
   } else {
      msg += "success";
   }

   l.fn(msg);
}

void boost::redis::detail::trace(const logger& l, std::string_view message)
{
   if (l.lvl < logger::level::debug)
      return;
   l.fn(message);
}

void boost::redis::detail::trace(const logger& l, std::string_view op, system::error_code const& ec)
{
   if (l.lvl < logger::level::debug)
      return;

   std::string msg{op};
   msg += ": ";
   format_error_code(ec, msg);

   l.fn(msg);
}
