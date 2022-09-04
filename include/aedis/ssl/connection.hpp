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

template <class AsyncReadWriteStream = boost::asio::ip::tcp::socket>
class connection :
   public connection_base<
      typename boost::asio::ssl::stream<AsyncReadWriteStream>::executor_type,
      connection<AsyncReadWriteStream>> {
public:
   /// Type of the next layer
   using next_layer_type = boost::asio::ssl::stream<AsyncReadWriteStream>;

   /// Executor type.
   using executor_type = typename next_layer_type::executor_type;

   using base_type = connection_base<executor_type, connection<AsyncReadWriteStream>>;

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
   , ex_{ex}
   {}

   explicit connection(boost::asio::io_context& ioc, config cfg = config{})
   : connection(ioc.get_executor(), std::move(cfg))
   { }

   /// Get the config object.
   auto get_config() noexcept -> config& { return cfg_;}

   /// Gets the config object.
   auto get_config() const noexcept -> config const& { return cfg_;}

   auto& lowest_layer() noexcept
   {
      BOOST_ASSERT(!!stream_);
      return stream_->lowest_layer();
   }

   auto& stream() noexcept
   {
      BOOST_ASSERT(!!stream_);
      return *stream_;
   }

   auto const& stream() const noexcept
   {
      BOOST_ASSERT(!!stream_);
      return *stream_;
   }

   void close_if_valid()
   {
      if (stream_)
         stream_->next_layer().close();
   }

   auto is_open() const noexcept
   {
      return stream_ != nullptr && stream_->next_layer().is_open();
   }

   auto is_null() const noexcept
   {
      return stream_ == nullptr;
   }

   void create_stream()
   {
      stream_ = std::make_shared<next_layer_type>(ex_, ctx_);
   }

private:
   config cfg_;
   executor_type ex_;
   boost::asio::ssl::context ctx_{boost::asio::ssl::context::sslv23};
   std::shared_ptr<boost::asio::ssl::stream<AsyncReadWriteStream>> stream_;
};

} // aedis::ssl

#endif // AEDIS_SSL_CONNECTION_HPP
