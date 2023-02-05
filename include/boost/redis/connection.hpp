/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONNECTION_HPP
#define BOOST_REDIS_CONNECTION_HPP

#include <boost/redis/detail/connection_base.hpp>
#include <boost/redis/response.hpp>
#include <boost/asio/io_context.hpp>

#include <chrono>
#include <memory>

namespace boost::redis {

/** @brief A connection to the Redis server.
 *  @ingroup high-level-api
 *
 *  For more details, please see the documentation of each individual
 *  function.
 *
 *  @tparam Socket The socket type e.g. asio::ip::tcp::socket.
 */
template <class Socket>
class basic_connection :
   private detail::connection_base<
      typename Socket::executor_type,
      basic_connection<Socket>> {
public:
   /// Executor type.
   using executor_type = typename Socket::executor_type;

   /// Type of the next layer
   using next_layer_type = Socket;

   /// Rebinds the socket type to another executor.
   template <class Executor1>
   struct rebind_executor
   {
      /// The socket type when rebound to the specified executor.
      using other = basic_connection<typename next_layer_type::template rebind_executor<Executor1>::other>;
   };

   using base_type = detail::connection_base<executor_type, basic_connection<Socket>>;

   /// Contructs from an executor.
   explicit
   basic_connection(executor_type ex)
   : base_type{ex}
   , stream_{ex}
   {}

   /// Contructs from a context.
   explicit
   basic_connection(asio::io_context& ioc)
   : basic_connection(ioc.get_executor())
   { }

   /// Returns the associated executor.
   auto get_executor() {return stream_.get_executor();}

   /// Resets the underlying stream.
   void reset_stream()
   {
      if (stream_.is_open()) {
         system::error_code ignore;
         stream_.shutdown(asio::ip::tcp::socket::shutdown_both, ignore);
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
    *  void f(system::error_code);
    *  @endcode
    *
    *  This function will complete when the connection is lost. If the
    *  error is asio::error::eof this function will complete
    *  without error.
    */
   template <class CompletionToken = asio::default_completion_token_t<executor_type>>
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
    *  @param response Response object.
    *  @param token Asio completion token.
    *
    *  For an example see cpp20_echo_server.cpp. The completion token must
    *  have the following signature
    *
    *  @code
    *  void f(system::error_code, std::size_t);
    *  @endcode
    *
    *  Where the second parameter is the size of the response in
    *  bytes.
    */
   template <
      class Response = ignore_t,
      class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_exec(
      request const& req,
      Response& response = ignore,
      CompletionToken token = CompletionToken{})
   {
      return base_type::async_exec(req, response, std::move(token));
   }

   /** @brief Receives server side pushes asynchronously.
    *
    *  Users that expect server pushes should call this function in a
    *  loop. If a push arrives and there is no reader, the connection
    *  will hang.
    *
    *  @param response The response object.
    *  @param token The Asio completion token.
    *
    *  For an example see cpp20_subscriber.cpp. The completion token must
    *  have the following signature
    *
    *  @code
    *  void f(system::error_code, std::size_t);
    *  @endcode
    *
    *  Where the second parameter is the size of the push in
    *  bytes.
    */
   template <
      class Response = ignore_t,
      class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_receive(
      Response& response = ignore,
      CompletionToken token = CompletionToken{})
   {
      return base_type::async_receive(response, std::move(token));
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

   /// Sets the maximum size of the read buffer.
   void set_max_buffer_read_size(std::size_t max_read_size) noexcept
      { base_type::set_max_buffer_read_size(max_read_size); }

   /** @brief Reserve memory on the read and write internal buffers.
    *
    *  This function will call `std::string::reserve` on the
    *  underlying buffers.
    *  
    *  @param read The new capacity of the read buffer.
    *  @param write The new capacity of the write buffer.
    */
   void reserve(std::size_t read, std::size_t write)
      { base_type::reserve(read, write); }

private:
   using this_type = basic_connection<next_layer_type>;

   template <class, class> friend class detail::connection_base;
   template <class, class> friend struct detail::exec_read_op;
   template <class, class> friend struct detail::exec_op;
   template <class, class> friend struct detail::receive_op;
   template <class> friend struct detail::reader_op;
   template <class> friend struct detail::writer_op;
   template <class> friend struct detail::run_op;
   template <class> friend struct detail::wait_receive_op;

   void close() { stream_.close(); }
   auto is_open() const noexcept { return stream_.is_open(); }
   auto lowest_layer() noexcept -> auto& { return stream_.lowest_layer(); }

   Socket stream_;
};

/** \brief A connection that uses a asio::ip::tcp::socket.
 *  \ingroup high-level-api
 */
using connection = basic_connection<asio::ip::tcp::socket>;

} // boost::redis

#endif // BOOST_REDIS_CONNECTION_HPP
