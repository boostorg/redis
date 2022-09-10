/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_CONNECTION_HPP
#define AEDIS_CONNECTION_HPP

#include <chrono>
#include <memory>

#include <boost/asio/io_context.hpp>
#include <aedis/connection_base.hpp>

namespace aedis {

/** @brief Connection to the Redis server over plain sockets.
 *  @ingroup any
 */
template <class AsyncReadWriteStream = boost::asio::ip::tcp::socket>
class connection :
   public connection_base<
      typename AsyncReadWriteStream::executor_type,
      connection<AsyncReadWriteStream>> {
public:
   /// Executor type.
   using executor_type = typename AsyncReadWriteStream::executor_type;

   /// Type of the next layer
   using next_layer_type = AsyncReadWriteStream;

   using base_type = connection_base<executor_type, connection<AsyncReadWriteStream>>;

   using this_type = connection<next_layer_type>;

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
   explicit connection(executor_type ex, config cfg = {})
   : base_type{ex}
   , cfg_{cfg}
   , stream_{ex}
   {}

   explicit connection(boost::asio::io_context& ioc, config cfg = config{})
   : connection(ioc.get_executor(), std::move(cfg))
   { }

   /// Get the config object.
   auto get_config() noexcept -> config& { return cfg_;}

   /// Gets the config object.
   auto get_config() const noexcept -> config const& { return cfg_;}

   /// Reset the underlying stream.
   void reset_stream()
   {
      if (stream_.is_open()) {
         boost::system::error_code ignore;
         stream_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore);
         stream_.close(ignore);
      }
   }

private:
   template <class, class> friend class connection_base;
   template <class, class> friend struct detail::exec_read_op;
   template <class, class> friend struct detail::exec_op;
   template <class, class> friend struct detail::receive_push_op;
   template <class> friend struct detail::ping_op;
   template <class> friend struct detail::check_idle_op;
   template <class> friend struct detail::reader_op;
   template <class> friend struct detail::writer_op;
   template <class> friend struct detail::connect_with_timeout_op;
   template <class> friend struct detail::run_op;

   template <class CompletionToken>
   auto async_connect(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::connect_with_timeout_op<this_type>{this}, token, stream_);
   }

   void close() { stream_.close(); }
   auto is_open() const noexcept { return stream_.is_open(); }
   auto& lowest_layer() noexcept { return stream_.lowest_layer(); }
   auto& next_layer() noexcept { return stream_; }
   auto const& next_layer() const noexcept { return stream_; }

   config cfg_;
   AsyncReadWriteStream stream_;
};

} // aedis

#endif // AEDIS_CONNECTION_HPP
