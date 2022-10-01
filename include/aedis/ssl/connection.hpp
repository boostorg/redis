/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_SSL_CONNECTION_HPP
#define AEDIS_SSL_CONNECTION_HPP

#include <chrono>
#include <memory>

#include <boost/asio/io_context.hpp>
#include <aedis/connection_base.hpp>
#include <aedis/ssl/detail/connection_ops.hpp>

namespace aedis::ssl {

template <class>
class connection;

/** @brief A SSL connection to the Redis server.
 *  @ingroup any
 *
 *  This class keeps a healthy connection to the Redis instance where
 *  commands can be sent at any time. For more details, please see the
 *  documentation of each individual function.
 *
 *  @remarks This class exposes only asynchronous member functions,
 *  synchronous communications with the Redis server is provided by
 *  the `aedis::sync` class.
 *
 *  @tparam Derived class.
 *
 */
template <class AsyncReadWriteStream>
class connection<boost::asio::ssl::stream<AsyncReadWriteStream>> :
   public connection_base<
      typename boost::asio::ssl::stream<AsyncReadWriteStream>::executor_type,
      connection<boost::asio::ssl::stream<AsyncReadWriteStream>>> {
public:
   /// Type of the next layer
   using next_layer_type = boost::asio::ssl::stream<AsyncReadWriteStream>;

   /// Executor type.
   using executor_type = typename next_layer_type::executor_type;

   /** @brief Connection configuration parameters.
    */
   struct config {
      /// Timeout of the resolve operation.
      std::chrono::milliseconds resolve_timeout = std::chrono::seconds{10};

      /// Timeout of the connect operation.
      std::chrono::milliseconds connect_timeout = std::chrono::seconds{10};

      /// Timeout of the ssl handshake operation.
      std::chrono::milliseconds handshake_timeout = std::chrono::seconds{10};

      /// Timeout of the resp3 handshake operation.
      std::chrono::milliseconds resp3_handshake_timeout = std::chrono::seconds{2};

      /// Time interval of ping operations.
      std::chrono::milliseconds ping_interval = std::chrono::seconds{1};
   };

   /// Constructor
   explicit connection(executor_type ex, boost::asio::ssl::context& ctx, config cfg = {})
   : base_type{ex}
   , cfg_{cfg}
   , stream_{ex, ctx}
   {
   }

   /// Constructor
   explicit connection(boost::asio::io_context& ioc, boost::asio::ssl::context& ctx, config cfg = config{})
   : connection(ioc.get_executor(), ctx, std::move(cfg))
   { }

   /// Returns a reference to the configuration parameters.
   auto get_config() noexcept -> config& { return cfg_;}

   /// Returns a const reference to the configuration parameters.
   auto get_config() const noexcept -> config const& { return cfg_;}

   /// Reset the underlying stream.
   void reset_stream(boost::asio::ssl::context& ctx)
   {
      stream_ = next_layer_type{ex_, ctx};
   }

   /// Returns a reference to the next layer.
   auto& next_layer() noexcept { return stream_; }

   /// Returns a const reference to the next layer.
   auto const& next_layer() const noexcept { return stream_; }

private:
   using base_type = connection_base<executor_type, connection<boost::asio::ssl::stream<AsyncReadWriteStream>>>;
   using this_type = connection<next_layer_type>;

   template <class, class> friend class aedis::connection_base;
   template <class, class> friend struct aedis::detail::exec_op;
   template <class> friend struct detail::ssl_connect_with_timeout_op;
   template <class> friend struct aedis::detail::run_op;
   template <class> friend struct aedis::detail::writer_op;
   template <class> friend struct aedis::detail::ping_op;
   template <class> friend struct aedis::detail::check_idle_op;
   template <class> friend struct aedis::detail::reader_op;

   auto& lowest_layer() noexcept { return stream_.lowest_layer(); }
   auto is_open() const noexcept { return stream_.next_layer().is_open(); }
   void close() { stream_.next_layer().close(); }

   template <class CompletionToken>
   auto async_connect(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::ssl_connect_with_timeout_op<this_type>{this}, token, stream_);
   }

   config cfg_;
   executor_type ex_;
   next_layer_type stream_;
};

} // aedis::ssl

#endif // AEDIS_SSL_CONNECTION_HPP
