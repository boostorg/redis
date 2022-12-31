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
#include <aedis/detail/connection_base.hpp>

namespace aedis {

/** @brief A connection to the Redis server.
 *  @ingroup high-level-api
 *
 *  For more details, please see the documentation of each individual
 *  function.
 *
 *  @tparam AsyncReadWriteStream A stream that supports reading and
 *  writing.
 */
template <class AsyncReadWriteStream>
class basic_connection :
   private detail::connection_base<
      typename AsyncReadWriteStream::executor_type,
      basic_connection<AsyncReadWriteStream>> {
public:
   /// Executor type.
   using executor_type = typename AsyncReadWriteStream::executor_type;

   /// Type of the next layer
   using next_layer_type = AsyncReadWriteStream;

   /// Rebinds the socket type to another executor.
   template <class Executor1>
   struct rebind_executor
   {
      /// The socket type when rebound to the specified executor.
      using other = basic_connection<typename next_layer_type::template rebind_executor<Executor1>::other>;
   };

   using base_type = detail::connection_base<executor_type, basic_connection<AsyncReadWriteStream>>;

   /// Contructs from an executor.
   explicit
   basic_connection(
      executor_type ex,
      std::pmr::memory_resource* resource = std::pmr::get_default_resource())
   : base_type{ex, resource}
   , stream_{ex}
   {}

   /// Contructs from a context.
   explicit
   basic_connection(
      boost::asio::io_context& ioc,
      std::pmr::memory_resource* resource = std::pmr::get_default_resource())
   : basic_connection(ioc.get_executor(), resource)
   { }

   /// Returns the associated executor.
   auto get_executor() {return stream_.get_executor();}

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

   /** @brief Starts read and write operations
    *
    *  This function starts read and write operations with the Redis
    *  server. More specifically it will trigger the write of all
    *  requests i.e. calls to `async_exec` that happened prior to this
    *  call.
    *
    *  @param token Completion token.
    *
    *  The completion token must have the following signature
    *
    *  @code
    *  void f(boost::system::error_code);
    *  @endcode
    *
    *  This function will complete when the connection is lost. If the
    *  error is boost::asio::error::eof this function will complete
    *  without error.
    */
   template <class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto async_run(CompletionToken token = CompletionToken{})
   {
      return base_type::async_run(std::move(token));
   }

   /** @brief Executes a command on the Redis server asynchronously.
    *
    *  This function sends a request to the Redis server and
    *  complete after the response has been processed. If the request
    *  contains only commands that don't expect a response, the
    *  completion occurs after it has been written to the underlying
    *  stream.  Multiple concurrent calls to this function will be
    *  automatically queued by the implementation.
    *
    *  @param req Request object.
    *  @param adapter Response adapter.
    *  @param token Asio completion token.
    *
    *  For an example see cpp20_echo_server.cpp. The completion token must
    *  have the following signature
    *
    *  @code
    *  void f(boost::system::error_code, std::size_t);
    *  @endcode
    *
    *  Where the second parameter is the size of the response in
    *  bytes.
    */
   template <
      class Adapter = detail::response_traits<void>::adapter_type,
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
    *  Users that expect server pushes should call this function in a
    *  loop. If a push arrives and there is no reader, the connection
    *  will hang.
    *
    *  @param adapter The response adapter.
    *  @param token The Asio completion token.
    *
    *  For an example see cpp20_subscriber.cpp. The completion token must
    *  have the following signature
    *
    *  @code
    *  void f(boost::system::error_code, std::size_t);
    *  @endcode
    *
    *  Where the second parameter is the size of the push in
    *  bytes.
    */
   template <
      class Adapter = detail::response_traits<void>::adapter_type,
      class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto async_receive(
      Adapter adapter = adapt(),
      CompletionToken token = CompletionToken{})
   {
      return base_type::async_receive(adapter, std::move(token));
   }

   /** @brief Cancel operations.
    *
    *  @li `operation::exec`: Cancels operations started with
    *  `async_exec`. Affects only requests that haven't been written
    *  yet.
    *  @li operation::run: Cancels the `async_run` operation. Notice
    *  that the preferred way to close a connection is to send a
    *  [QUIT](https://redis.io/commands/quit/) command to the server.
    *  @li operation::receive: Cancels any ongoing calls to *  `async_receive`.
    *
    *  @param op: The operation to be cancelled.
    *  @returns The number of operations that have been canceled.
    */
   auto cancel(operation op) -> std::size_t
      { return base_type::cancel(op); }

private:
   using this_type = basic_connection<next_layer_type>;

   template <class, class> friend class detail::connection_base;
   template <class, class> friend struct detail::exec_read_op;
   template <class, class> friend struct detail::exec_op;
   template <class> friend struct detail::reader_op;
   template <class> friend struct detail::writer_op;
   template <class> friend struct detail::run_op;

   void close() { stream_.close(); }
   auto is_open() const noexcept { return stream_.is_open(); }
   auto lowest_layer() noexcept -> auto& { return stream_.lowest_layer(); }

   AsyncReadWriteStream stream_;
};

/** \brief A connection that uses a boost::asio::ip::tcp::socket.
 *  \ingroup high-level-api
 */
using connection = basic_connection<boost::asio::ip::tcp::socket>;

} // aedis

#endif // AEDIS_CONNECTION_HPP
