/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
 * Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */
#ifndef BOOST_REDIS_REDIS_STREAM_HPP
#define BOOST_REDIS_REDIS_STREAM_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

#include <utility>

namespace boost {
namespace redis {
namespace detail {

template <class Executor>
class redis_stream {
   asio::ssl::context ssl_ctx_;
   asio::ip::basic_resolver<asio::ip::tcp, Executor> resolv_;
   typename asio::steady_timer::template rebind_executor<Executor>::other timer_;
   asio::ssl::stream<asio::basic_stream_socket<asio::ip::tcp, Executor>> stream_;
   bool ssl_stream_used_{};
   bool use_ssl_{};

   void reset_stream() { stream_ = {resolv_.get_executor(), ssl_ctx_}; }

   struct connect_op {
      redis_stream& obj;
      const config* cfg;
      const logger* lgr;
      asio::coroutine coro{};

      // This overload will be used for connects. We only need the endpoint
      // for logging, so log it and call the coroutine
      template <class Self>
      void operator()(
         Self& self,
         system::error_code ec,
         const asio::ip::tcp::endpoint& selected_endpoint)
      {
         log_connect(*lgr, ec, selected_endpoint);
         (*this)(self, ec);
      }

      template <class Self>
      void operator()(
         Self& self,
         system::error_code ec = {},
         asio::ip::tcp::resolver::results_type resolver_results = {})
      {
         BOOST_ASIO_CORO_REENTER(coro)
         {
            // ssl::stream doesn't support being re-used. If we're to use
            // TLS and the stream has been used, re-create it.
            // Must be done before anything else is done on the stream
            if (cfg->use_ssl && obj.ssl_stream_used_)
               obj.reset_stream();

            // Resolve the server's address
            BOOST_ASIO_CORO_YIELD
            obj.resolv_.async_resolve(
               cfg->addr.host,
               cfg->addr.port,
               asio::cancel_after(obj.timer_, cfg->resolve_timeout, std::move(self)));

            // Log it
            log_resolve(*lgr, ec, resolver_results);

            // If this failed, we can't continue
            if (ec) {
               self.complete(ec == asio::error::operation_aborted ? error::resolve_timeout : ec);
               return;
            }

            // Connect to the address that the resolver provided us
            BOOST_ASIO_CORO_YIELD
            asio::async_connect(
               obj.stream_.next_layer(),
               std::move(resolver_results),
               asio::cancel_after(obj.timer_, cfg->connect_timeout, std::move(self)));

            // Note: logging is performed in the specialized operator() function.
            // If this failed, we can't continue
            if (ec) {
               self.complete(ec == asio::error::operation_aborted ? error::connect_timeout : ec);
               return;
            }

            if (cfg->use_ssl) {
               // Mark the SSL stream as used
               obj.ssl_stream_used_ = true;

               // If we were configured to use TLS, perform the handshake
               BOOST_ASIO_CORO_YIELD
               obj.stream_.async_handshake(
                  asio::ssl::stream_base::client,
                  asio::cancel_after(obj.timer_, cfg->ssl_handshake_timeout, std::move(self)));

               log_ssl_handshake(*lgr, ec);

               // If this failed, we can't continue
               if (ec) {
                  self.complete(
                     ec == asio::error::operation_aborted ? error::ssl_handshake_timeout : ec);
                  return;
               }

               // Record that we're using SSL
               obj.use_ssl_ = true;
            }

            // Done
            self.complete(system::error_code());
         }
      }
   };

public:
   explicit redis_stream(Executor ex, asio::ssl::context&& ssl_ctx)
   : ssl_ctx_{std::move(ssl_ctx)}
   , resolv_{ex}
   , timer_{ex}
   , stream_{std::move(ex), ssl_ctx_}
   { }

   // Executor. Required to satisfy the AsyncStream concept
   using executor_type = Executor;
   executor_type get_executor() noexcept { return resolv_.get_executor(); }

   // Accessors
   const auto& get_ssl_context() const noexcept { return ssl_ctx_; }
   bool is_open() const { return stream_.next_layer().is_open(); }
   auto& next_layer() { return stream_; }
   const auto& next_layer() const { return stream_; }

   // I/O
   template <class CompletionToken>
   auto async_connect(const config* cfg, const logger* l, CompletionToken&& token)
   {
      return asio::async_compose<CompletionToken, void(system::error_code)>(
         connect_op{*this, cfg, l},
         token);
   }

   template <class ConstBufferSequence, class CompletionToken>
   auto async_write_some(const ConstBufferSequence& buffers, CompletionToken&& token)
   {
      if (use_ssl_) {
         return stream_.async_write_some(buffers, std::forward<CompletionToken>(token));
      } else {
         return stream_.next_layer().async_write_some(
            buffers,
            std::forward<CompletionToken>(token));
      }
   }

   template <class MutableBufferSequence, class CompletionToken>
   auto async_read_some(const MutableBufferSequence& buffers, CompletionToken&& token)
   {
      if (use_ssl_) {
         return stream_.async_read_some(buffers, std::forward<CompletionToken>(token));
      } else {
         return stream_.next_layer().async_read_some(buffers, std::forward<CompletionToken>(token));
      }
   }

   // Cleanup
   void cancel_resolve() { resolv_.cancel(); }

   void close()
   {
      if (stream_.next_layer().is_open()) {
         system::error_code ec;
         stream_.next_layer().close(ec);
      }
   }
};

}  // namespace detail
}  // namespace redis
}  // namespace boost

#endif
