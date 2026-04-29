//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_CO_REDIS_STREAM_HPP
#define BOOST_REDIS_CO_REDIS_STREAM_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connect_params.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/transport_type.hpp>
#include <boost/redis/impl/log_utils.hpp>
#include <boost/redis/logger.hpp>

#include <boost/assert.hpp>
#include <boost/capy/ex/execution_context.hpp>
#include <boost/capy/io/any_stream.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/timeout.hpp>
#include <boost/corosio/connect.hpp>
#include <boost/corosio/local_stream_socket.hpp>
#include <boost/corosio/openssl_stream.hpp>
#include <boost/corosio/resolver.hpp>
#include <boost/corosio/resolver_results.hpp>
#include <boost/corosio/tcp_socket.hpp>
#include <boost/corosio/timer.hpp>
#include <boost/corosio/tls_context.hpp>
#include <boost/system/error_code.hpp>

#include <optional>
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

struct co_redis_stream {
   struct tcp_state {
      corosio::resolver resolv;
      corosio::tcp_socket sock;

      explicit tcp_state(capy::execution_context& ctx)
      : resolv(ctx)
      , sock(ctx)
      { }
   };

   // Required to create the other objects
   capy::execution_context& ctx_;
   corosio::tls_context tls_ctx_;

   // Constructed lazily as required
   std::optional<tcp_state> tcp_;
   std::optional<corosio::local_stream_socket> unix_;
   std::optional<corosio::openssl_stream> tls_;

   // Contains the stream that will end up being used
   capy::any_stream stream_;

   void setup_unix()
   {
      if (unix_.has_value()) {
         // UNIX sockets don't use range connect.
         // We need to close and re-open the socket before establishing another connection
         unix_->close();
         unix_->open();
      } else {
         unix_.emplace(ctx_);
      }
   }

   void setup_tcp()
   {
      // Allocate the object if not there.
      // TCP uses range connect, so we don't need to close and reopen the socket
      if (!tcp_.has_value())
         tcp_.emplace(ctx_);
   }

   void setup_tls()
   {
      if (tls_.has_value())
         tls_->reset();
      else
         tls_.emplace(capy::any_stream(&tcp_->sock), tls_ctx_);
   }

   auto unix_connect(const connect_params& params)
   {
      return capy::timeout(
         unix_->connect(corosio::local_endpoint(params.addr.unix_socket())),
         params.connect_timeout);
   }

   auto tcp_resolve(const connect_params& params)
   {
      return capy::timeout(
         tcp_->resolv.resolve(params.addr.tcp_address().host, params.addr.tcp_address().port),
         params.resolve_timeout);
   }

   auto tcp_connect(const connect_params& params, const corosio::resolver_results& results)
   {
      // TODO: prevent copy here
      return capy::timeout(corosio::connect(tcp_->sock, results), params.connect_timeout);
   }

   auto tls_handshake(const connect_params& params)
   {
      return capy::timeout(
         tls_->handshake(corosio::tls_stream::handshake_type::client),
         params.ssl_handshake_timeout);
   }

   capy::io_task<> connect(const connect_params& params, buffered_logger& lgr)
   {
      auto type = params.addr.type();

      if (type == transport_type::unix_socket) {
         // Setup
         setup_unix();

         // Actual connect
         auto [ec] = co_await unix_connect(params);
         if (ec) {
            log_info(lgr, "Connect: UNIX socket connect failed: ", system::error_code(ec));
            co_return {ec};
         }
         log_debug(lgr, "Connect: UNIX socket connect succeeded");

         // Done
         stream_ = capy::any_stream(&*unix_);
         co_return {};

      } else {
         // TCP (with or without TLS)
         setup_tcp();

         // Resolve names
         auto [ec, endpoints] = co_await tcp_resolve(params);
         if (ec) {
            log_info(lgr, "Connect: hostname resolution failed: ", system::error_code(ec));
            co_return {ec};
         }
         log_debug(lgr, "Connect: hostname resolution results: ", endpoints);

         // Now connect to the endpoints returned by the resolver
         auto [ec_connect, selected_endpoint] = co_await tcp_connect(params, endpoints);
         if (ec_connect) {
            log_info(lgr, "Connect: TCP connect failed: ", system::error_code(ec_connect));
            co_return {ec_connect};
         }
         log_debug(lgr, "Connect: TCP connect succeeded. Selected endpoint: ", selected_endpoint);

         // If not using TLS, we're done
         if (type == transport_type::tcp) {
            stream_ = capy::any_stream(&tcp_->sock);
            co_return {};
         }

         // Set up a working TLS stream
         setup_tls();

         // TLS handshake
         auto [ec_handshake] = co_await tls_handshake(params);
         if (ec_handshake) {
            log_info(lgr, "Connect: SSL handshake failed: ", system::error_code(ec_handshake));
            co_return {ec_handshake};
         }
         log_debug(lgr, "Connect: SSL handshake succeeded");

         // Done
         stream_ = capy::any_stream(&*tls_);
         co_return {};
      }
   }

   explicit co_redis_stream(capy::execution_context& ctx, corosio::tls_context tls_ctx)
   : ctx_(ctx)
   , tls_ctx_(std::move(tls_ctx))
   { }

   template <capy::ConstBufferSequence BuffType>
   auto write_some(BuffType&& buffers)
   {
      return stream_.write_some(std::forward<BuffType>(buffers));
   }

   template <capy::MutableBufferSequence BuffType>
   auto read_some(BuffType&& buffers)
   {
      return stream_.read_some(std::forward<BuffType>(buffers));
   }
};

}  // namespace boost::redis::detail

#endif
