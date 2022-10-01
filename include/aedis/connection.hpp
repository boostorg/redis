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

/** @brief A connection to the Redis server.
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

   /** @brief Connection configuration parameters.
    */
   struct config {
      /// Timeout of the resolve operation.
      std::chrono::milliseconds resolve_timeout = std::chrono::seconds{10};

      /// Timeout of the connect operation.
      std::chrono::milliseconds connect_timeout = std::chrono::seconds{10};

      /// Timeout of the resp3 handshake operation.
      std::chrono::milliseconds resp3_handshake_timeout = std::chrono::seconds{2};

      /// Time interval of ping operations.
      std::chrono::milliseconds ping_interval = std::chrono::seconds{1};

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

   /// Returns a reference to the configuration parameters.
   auto get_config() noexcept -> config& { return cfg_;}

   /// Returns a const reference to the configuration parameters.
   auto get_config() const noexcept -> config const& { return cfg_;}

   /// Resets the underlying stream.
   void reset_stream()
   {
      if (stream_.is_open()) {
         boost::system::error_code ignore;
         stream_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore);
         stream_.close(ignore);
      }
   }

   /// Returns a reference to the next layer.
   auto next_layer() noexcept -> auto& { return stream_; }

   /// Returns a const reference to the next layer.
   auto next_layer() const noexcept -> auto const& { return stream_; }

private:
   using base_type = connection_base<executor_type, connection<AsyncReadWriteStream>>;
   using this_type = connection<next_layer_type>;

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

   config cfg_;
   AsyncReadWriteStream stream_;
};

} // aedis

#endif // AEDIS_CONNECTION_HPP
