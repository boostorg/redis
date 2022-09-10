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

namespace aedis::ssl {

template <class>
class connection;

/** @brief Connection to the Redis server over SSL sockets.
 *  @ingroup any
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

   using base_type = connection_base<executor_type, connection<boost::asio::ssl::stream<AsyncReadWriteStream>>>;

   /** @brief Connection configuration parameters.
    */
   struct config {
      /// Timeout of the resolve operation.
      std::chrono::milliseconds resolve_timeout = std::chrono::seconds{10};

      /// Timeout of the connect operation.
      std::chrono::milliseconds connect_timeout = std::chrono::seconds{10};

      /// Time interval of ping operations.
      std::chrono::milliseconds ping_interval = std::chrono::seconds{1};

      /// The maximum size of read operations.
      std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)();

      /// Whether to coalesce requests (see [pipelines](https://redis.io/topics/pipelining)).
      bool coalesce_requests = true;
   };

   /// Constructor
   explicit connection(executor_type ex, boost::asio::ssl::context& ctx, config cfg = {})
   : base_type{ex}
   , cfg_{cfg}
   , stream_{ex, ctx}
   {
   }

   explicit connection(boost::asio::io_context& ioc, boost::asio::ssl::context& ctx, config cfg = config{})
   : connection(ioc.get_executor(), ctx, std::move(cfg))
   { }

   /// Get the config object.
   auto get_config() noexcept -> config& { return cfg_;}

   /// Gets the config object.
   auto get_config() const noexcept -> config const& { return cfg_;}

   /// Reset the underlying stream.
   void reset_stream(boost::asio::ssl::context& ctx)
   {
      stream_ = next_layer_type{ex_, ctx};
   }

   auto& next_layer() noexcept { return stream_; }
   auto const& next_layer() const noexcept { return stream_; }

   // TODO: Make this private.
   void close() { stream_.next_layer().close(); }
   auto& lowest_layer() noexcept { return stream_.lowest_layer(); }
   auto is_open() const noexcept { return stream_.next_layer().is_open(); }

   template <
      class EndpointSequence,
      class CompletionToken
      >
   auto async_connect(
         detail::conn_timer_t<executor_type>& timer,
         EndpointSequence ep,
         CompletionToken&& token)
   {
      return detail::async_connect(lowest_layer(), timer, ep, std::move(token));
   }

private:
   config cfg_;
   executor_type ex_;
   next_layer_type stream_;
};

} // aedis::ssl

#endif // AEDIS_SSL_CONNECTION_HPP
