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

namespace boost::redis {

void detail::fix_default_logger(logger& l, std::string_view prefix)
{
   if (!l.fn) {
      l.fn = [owning_prefix = std::string(prefix)](std::string_view msg) {
         std::clog << owning_prefix << msg << std::endl;
      };
   }
}

namespace detail {

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
   to += std::to_string(ep.port());  // TODO: we could probably use to_chars here
}

}  // namespace detail

void detail::log_resolve(
   const logger& l,
   system::error_code const& ec,
   asio::ip::tcp::resolver::results_type const& res)
{
   if (l.lvl < log_level::info)
      return;

   // TODO: can we make this non-allocating?
   std::string msg;

   if (ec) {
      msg += "Error resolving the server hostname: ";
      msg += ec.what();
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

void detail::log_connect(
   const logger& l,
   system::error_code const& ec,
   asio::ip::tcp::endpoint const& ep)
{
   if (l.lvl < log_level::info)
      return;

   std::string msg;

   if (ec) {
      msg += "Failed connecting to the server: ";
      msg += ec.what();
   } else {
      msg += "Connected to ";
      format_tcp_endpoint(ep, msg);
   }

   l.fn(msg);
}

void detail::log_ssl_handshake(const logger& l, system::error_code const& ec)
{
   if (l.lvl < log_level::info)
      return;

   std::string msg{"SSL handshake: "};
   msg += ec.what();

   l.fn(msg);
}

void detail::log_write(const logger& l, system::error_code const& ec, std::string_view payload)
{
   if (l.lvl < log_level::info)
      return;

   std::string msg{"writer_op: "};
   if (ec) {
      msg += ec.what();
   } else {
      msg += std::to_string(payload.size());
      msg += " bytes written.";
   }

   l.fn(msg);
}

void detail::log_read(const logger& l, system::error_code const& ec, std::size_t n)
{
   if (l.lvl < log_level::info)
      return;

   std::string msg{"reader_op: "};
   if (ec) {
      msg += ec.what();
   } else {
      msg += std::to_string(n);
      msg += " bytes read.";
   }

   l.fn(msg);
}

void detail::log_hello(const logger& l, system::error_code const& ec, generic_response const& resp)
{
   if (l.lvl < log_level::info)
      return;

   std::string msg{"hello_op: "};
   if (ec) {
      msg += ec.what();
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

void detail::trace(const logger& l, std::string_view message)
{
   if (l.lvl < log_level::debug)
      return;
   l.fn(message);
}

void detail::trace(const logger& l, std::string_view op, system::error_code const& ec)
{
   if (l.lvl < log_level::debug)
      return;

   std::string msg{op};
   msg += ": ";
   msg += ec.what();

   l.fn(msg);
}

}  // namespace boost::redis
