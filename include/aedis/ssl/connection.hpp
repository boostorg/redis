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
#include <aedis/detail/connection_base.hpp>
#include <aedis/ssl/detail/connection_ops.hpp>

namespace aedis::ssl {

template <class>
class connection;

/** \brief A SSL connection to the Redis server.
 *  \ingroup high-level-api
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
   private aedis::detail::connection_base<
      typename boost::asio::ssl::stream<AsyncReadWriteStream>::executor_type,
      connection<boost::asio::ssl::stream<AsyncReadWriteStream>>> {
public:
   /// Type of the next layer
   using next_layer_type = boost::asio::ssl::stream<AsyncReadWriteStream>;

   /// Executor type.
   using executor_type = typename next_layer_type::executor_type;
   using base_type = aedis::detail::connection_base<executor_type, connection<boost::asio::ssl::stream<AsyncReadWriteStream>>>;

   /** \brief Connection configuration parameters.
    */
   struct timeouts {
      /// Timeout of the resolve operation.
      std::chrono::steady_clock::duration resolve_timeout = std::chrono::seconds{10};

      /// Timeout of the connect operation.
      std::chrono::steady_clock::duration connect_timeout = std::chrono::seconds{10};

      /// Timeout of the ssl handshake operation.
      std::chrono::steady_clock::duration handshake_timeout = std::chrono::seconds{10};

      /// Timeout of the resp3 handshake operation.
      std::chrono::steady_clock::duration resp3_handshake_timeout = std::chrono::seconds{2};

      /// Time interval of ping operations.
      std::chrono::steady_clock::duration ping_interval = std::chrono::seconds{1};
   };

   /// Constructor
   explicit connection(executor_type ex, boost::asio::ssl::context& ctx)
   : base_type{ex}
   , stream_{ex, ctx}
   {
   }

   /// Constructor
   explicit connection(boost::asio::io_context& ioc, boost::asio::ssl::context& ctx)
   : connection(ioc.get_executor(), ctx)
   { }

   /// Returns the associated executor.
   auto get_executor() {return stream_.get_executor();}

   /// Reset the underlying stream.
   void reset_stream(boost::asio::ssl::context& ctx)
   {
      stream_ = next_layer_type{stream_.get_executor(), ctx};
   }

   /// Returns a reference to the next layer.
   auto& next_layer() noexcept { return stream_; }

   /// Returns a const reference to the next layer.
   auto const& next_layer() const noexcept { return stream_; }

   /** @brief Connects and executes a request asynchronously.
    *
    *  See aedis::connection::async_run for detailed information.
    */
   template <class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto
   async_run(
      endpoint ep,
      timeouts ts = timeouts{},
      CompletionToken token = CompletionToken{})
   {
      return base_type::async_run(ep, ts, std::move(token));
   }

   /** @brief Connects and executes a request asynchronously.
    *
    *  See aedis::connection::async_run for detailed information.
    */
   template <
      class Adapter = aedis::detail::response_traits<void>::adapter_type,
      class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto async_run(
      endpoint ep,
      resp3::request const& req,
      Adapter adapter,
      timeouts ts,
      CompletionToken token = CompletionToken{})
   {
      return base_type::async_run(ep, req, adapter, ts, std::move(token));
   }

   /** @brief Executes a command on the Redis server asynchronously.
    *
    *  See aedis::connection::async_exec for detailed information.
    */
   template <
      class Adapter = aedis::detail::response_traits<void>::adapter_type,
      class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto async_exec(
      resp3::request const& req,
      Adapter adapter = adapt(),
      CompletionToken token = CompletionToken{})
   {
      return base_type::async_exec(req, adapter, std::move(token));
   }

   /** @brief Receives server side pushes asynchronously.
    *
    *  See aedis::connection::async_receive_push for detailed information.
    */
   template <
      class Adapter = aedis::detail::response_traits<void>::adapter_type,
      class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto async_receive_push(
      Adapter adapter = adapt(),
      CompletionToken token = CompletionToken{})
   {
      return base_type::async_receive_push(adapter, std::move(token));
   }

   /** @brief Cancel operations.
    *
    *  See aedis::connection::cancel for detailed information.
    */
   auto cancel(operation op) -> std::size_t
      { return base_type::cancel(op); }

private:
   using this_type = connection<next_layer_type>;

   template <class, class> friend class aedis::detail::connection_base;
   template <class, class> friend struct aedis::detail::exec_op;
   template <class, class> friend struct detail::ssl_connect_with_timeout_op;
   template <class> friend struct aedis::detail::run_op;
   template <class> friend struct aedis::detail::writer_op;
   template <class> friend struct aedis::detail::check_idle_op;
   template <class> friend struct aedis::detail::reader_op;
   template <class, class> friend struct aedis::detail::exec_read_op;
   template <class> friend struct aedis::detail::ping_op;

   auto& lowest_layer() noexcept { return stream_.lowest_layer(); }
   auto is_open() const noexcept { return stream_.next_layer().is_open(); }
   void close() { stream_.next_layer().close(); }

   template <class Timer, class CompletionToken>
   auto
   async_connect(
      boost::asio::ip::tcp::resolver::results_type const& endpoints,
      timeouts ts,
      Timer& timer,
      CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::ssl_connect_with_timeout_op<this_type, Timer>{this, &endpoints, ts, &timer}, token, stream_);
   }

   next_layer_type stream_;
};

} // aedis::ssl

#endif // AEDIS_SSL_CONNECTION_HPP
