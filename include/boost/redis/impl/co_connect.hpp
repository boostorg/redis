//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_CO_CONNECT_HPP
#define BOOST_REDIS_CO_CONNECT_HPP

#include <boost/redis/detail/connect_params.hpp>
#include <boost/redis/impl/log_utils.hpp>
#include <boost/redis/logger.hpp>

#include <boost/capy/io_task.hpp>
#include <boost/corosio/endpoint.hpp>
#include <boost/corosio/resolver_results.hpp>

#include <string>

namespace boost::redis::detail {

// Logging
inline void format_tcp_endpoint(const corosio::endpoint& ep, std::string& to)
{
   if (ep.is_v6()) {
      to += '[';
      to += ep.v6_address().to_string();
      to += ']';
   } else {
      to += ep.v4_address().to_string();
   }
   to += ':';
   to += std::to_string(ep.port());
}

template <>
struct log_traits<corosio::endpoint> {
   static inline void log(std::string& to, const corosio::endpoint& value)
   {
      format_tcp_endpoint(value, to);
   }
};

template <>
struct log_traits<corosio::resolver_results> {
   static inline void log(std::string& to, const corosio::resolver_results& value)
   {
      auto iter = value.begin();
      auto end = value.end();

      if (iter != end) {
         format_tcp_endpoint(iter->get_endpoint(), to);
         ++iter;
         for (; iter != end; ++iter) {
            to += ", ";
            format_tcp_endpoint(iter->get_endpoint(), to);
         }
      }
   }
};

// Templatized for testing purposes
template <class StreamImpl>
capy::io_task<> co_connect(StreamImpl& impl, const connect_params& params, buffered_logger& lgr)
{
   auto type = params.addr.type();

   if (type == transport_type::unix_socket) {
      // Setup
      impl.setup_unix();

      // Actual connect
      auto [ec] = co_await impl.unix_connect(params);
      if (ec) {
         log_info(lgr, "Connect: UNIX socket connect failed: ", system::error_code(ec));
         co_return {ec};
      }
      log_debug(lgr, "Connect: UNIX socket connect succeeded");

      // Done
      co_return {};

   } else {
      // TCP (with or without TLS)
      if (type == transport_type::tcp_tls)
         impl.setup_tcp_tls();
      else
         impl.setup_tcp();

      // Resolve names
      auto [ec, endpoints] = co_await impl.tcp_resolve(params);
      if (ec) {
         log_info(lgr, "Connect: hostname resolution failed: ", system::error_code(ec));
         co_return {ec};
      }
      log_debug(lgr, "Connect: hostname resolution results: ", endpoints);

      // Now connect to the endpoints returned by the resolver
      auto [ec_connect, selected_endpoint] = co_await impl.tcp_connect(params, endpoints);
      if (ec_connect) {
         log_info(lgr, "Connect: TCP connect failed: ", system::error_code(ec_connect));
         co_return {ec_connect};
      }
      log_debug(lgr, "Connect: TCP connect succeeded. Selected endpoint: ", selected_endpoint);

      // If using TLS, perform the handshake
      if (type == transport_type::tcp_tls) {
         // TLS handshake
         auto [ec_handshake] = co_await impl.tls_handshake(params);
         if (ec_handshake) {
            log_info(lgr, "Connect: SSL handshake failed: ", system::error_code(ec_handshake));
            co_return {ec_handshake};
         }
         log_debug(lgr, "Connect: SSL handshake succeeded");
      }

      // Done
      co_return {};
   }
}

}  // namespace boost::redis::detail

#endif
