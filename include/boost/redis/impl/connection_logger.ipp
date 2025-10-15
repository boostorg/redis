/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/connection_logger.hpp>
#include <boost/redis/detail/exec_fsm.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <string>
#include <string_view>

namespace boost::redis::detail {

#define BOOST_REDIS_EXEC_SWITCH_CASE(elem) \
   case exec_action_type::elem: return "exec_action_type::" #elem

auto to_string(exec_action_type t) noexcept -> char const*
{
   switch (t) {
      BOOST_REDIS_EXEC_SWITCH_CASE(setup_cancellation);
      BOOST_REDIS_EXEC_SWITCH_CASE(immediate);
      BOOST_REDIS_EXEC_SWITCH_CASE(done);
      BOOST_REDIS_EXEC_SWITCH_CASE(notify_writer);
      BOOST_REDIS_EXEC_SWITCH_CASE(wait_for_response);
      default: return "exec_action_type::<invalid type>";
   }
}

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
      msg_ = "Failed to connect to the server: ";
      format_error_code(ec, msg_);
   } else {
      msg_ = "Connected to ";
      format_tcp_endpoint(ep, msg_);
   }

   logger_.fn(logger::level::info, msg_);
}

void connection_logger::on_connect(system::error_code const& ec, std::string_view unix_socket_ep)
{
   if (logger_.lvl < logger::level::info)
      return;

   if (ec) {
      msg_ = "Failed to connect to the server: ";
      format_error_code(ec, msg_);
   } else {
      msg_ = "Connected to ";
      msg_ += unix_socket_ep;
   }

   logger_.fn(logger::level::info, msg_);
}

void connection_logger::on_ssl_handshake(system::error_code const& ec)
{
   if (logger_.lvl < logger::level::info)
      return;

   if (ec) {
      msg_ = "Failed to perform SSL handshake: ";
      format_error_code(ec, msg_);
   } else {
      msg_ = "Successfully performed SSL handshake";
   }

   logger_.fn(logger::level::info, msg_);
}

void connection_logger::on_write(system::error_code const& ec, std::size_t n)
{
   if (logger_.lvl < logger::level::info)
      return;

   if (ec) {
      msg_ = "Writer task error: ";
      format_error_code(ec, msg_);
   } else {
      msg_ = "Writer task: ";
      msg_ += std::to_string(n);
      msg_ += " bytes written.";
   }

   logger_.fn(logger::level::info, msg_);
}

void connection_logger::on_read(system::error_code const& ec, std::size_t bytes_read)
{
   if (logger_.lvl < logger::level::debug)
      return;

   msg_ = "Reader task: ";
   msg_ += std::to_string(bytes_read);
   msg_ += " bytes read";
   if (ec) {
      msg_ += ", error: ";
      format_error_code(ec, msg_);
   }

   logger_.fn(logger::level::debug, msg_);
}

void connection_logger::on_setup(system::error_code const& ec, generic_response const& resp)
{
   if (logger_.lvl < logger::level::info)
      return;

   msg_ = "Setup request execution: ";
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

void connection_logger::log(logger::level lvl, std::string_view message)
{
   if (logger_.lvl < lvl)
      return;
   logger_.fn(lvl, message);
}

void connection_logger::log(logger::level lvl, std::string_view message1, std::string_view message2)
{
   if (logger_.lvl < lvl)
      return;

   msg_ = message1;
   msg_ += ": ";
   msg_ += message2;

   logger_.fn(lvl, msg_);
}

void connection_logger::log(logger::level lvl, std::string_view op, system::error_code const& ec)
{
   if (logger_.lvl < lvl)
      return;

   msg_ = op;
   msg_ += ": ";
   format_error_code(ec, msg_);

   logger_.fn(lvl, msg_);
}

}  // namespace boost::redis::detail
