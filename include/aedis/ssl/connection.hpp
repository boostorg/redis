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

namespace aedis::ssl {

template <class>
class basic_connection;

/** \brief A SSL connection to the Redis server.
 *  \ingroup high-level-api
 *
 *  This class keeps a healthy connection to the Redis instance where
 *  commands can be sent at any time. For more details, please see the
 *  documentation of each individual function.
 *
 *  @tparam AsyncReadWriteStream A stream that supports reading and
 *  writing.
 *
 */
template <class AsyncReadWriteStream>
class basic_connection<boost::asio::ssl::stream<AsyncReadWriteStream>> :
   private aedis::detail::connection_base<
      typename boost::asio::ssl::stream<AsyncReadWriteStream>::executor_type,
      basic_connection<boost::asio::ssl::stream<AsyncReadWriteStream>>> {
public:
   /// Type of the next layer
   using next_layer_type = boost::asio::ssl::stream<AsyncReadWriteStream>;

   /// Executor type.
   using executor_type = typename next_layer_type::executor_type;

   /// Rebinds the socket type to another executor.
   template <class Executor1>
   struct rebind_executor
   {
      /// The socket type when rebound to the specified executor.
      using other = basic_connection<boost::asio::ssl::stream<typename AsyncReadWriteStream::template rebind_executor<Executor1>::other>>;
   };

   using base_type = aedis::detail::connection_base<executor_type, basic_connection<boost::asio::ssl::stream<AsyncReadWriteStream>>>;

   /// Constructor
   explicit
   basic_connection(
      executor_type ex,
      boost::asio::ssl::context& ctx,
      std::pmr::memory_resource* resource = std::pmr::get_default_resource())
   : base_type{ex, resource}
   , stream_{ex, ctx}
   { }

   /// Constructor
   explicit
   basic_connection(
      boost::asio::io_context& ioc,
      boost::asio::ssl::context& ctx,
      std::pmr::memory_resource* resource = std::pmr::get_default_resource())
   : basic_connection(ioc.get_executor(), ctx, resource)
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

   /** @brief Establishes a connection with the Redis server asynchronously.
    *
    *  See aedis::connection::async_run for more information.
    */
   template <class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto async_run(CompletionToken token = CompletionToken{})
   {
      return base_type::async_run(std::move(token));
   }

   /** @brief Executes a command on the Redis server asynchronously.
    *
    *  See aedis::connection::async_exec for more information.
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
    *  See aedis::connection::async_receive for detailed information.
    */
   template <
      class Adapter = aedis::detail::response_traits<void>::adapter_type,
      class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto async_receive(
      Adapter adapter = adapt(),
      CompletionToken token = CompletionToken{})
   {
      return base_type::async_receive(adapter, std::move(token));
   }

   /** @brief Cancel operations.
    *
    *  See aedis::connection::cancel for more information.
    */
   auto cancel(operation op) -> std::size_t
      { return base_type::cancel(op); }

   auto& lowest_layer() noexcept { return stream_.lowest_layer(); }

private:
   using this_type = basic_connection<next_layer_type>;

   template <class, class> friend class aedis::detail::connection_base;
   template <class, class> friend struct aedis::detail::exec_op;
   template <class, class> friend struct aedis::detail::run_op;
   template <class> friend struct aedis::detail::writer_op;
   template <class> friend struct aedis::detail::reader_op;
   template <class, class> friend struct aedis::detail::exec_read_op;

   auto is_open() const noexcept { return stream_.next_layer().is_open(); }
   void close() { stream_.next_layer().close(); }

   next_layer_type stream_;
};

/** \brief A connection that uses a boost::asio::ssl::stream<boost::asio::ip::tcp::socket>.
 *  \ingroup high-level-api
 */
using connection = basic_connection<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>;

} // aedis::ssl

#endif // AEDIS_SSL_CONNECTION_HPP
