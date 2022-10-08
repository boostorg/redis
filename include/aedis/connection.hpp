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
   private detail::connection_base<
      typename AsyncReadWriteStream::executor_type,
      connection<AsyncReadWriteStream>> {
public:
   /// Executor type.
   using executor_type = typename AsyncReadWriteStream::executor_type;

   /// Type of the next layer
   using next_layer_type = AsyncReadWriteStream;
   using base_type = detail::connection_base<executor_type, connection<AsyncReadWriteStream>>;

   /** \brief Connection configuration parameters.
    */
   struct timeouts {
      /// Timeout of the resolve operation.
      std::chrono::steady_clock::duration resolve_timeout = std::chrono::seconds{10};

      /// Timeout of the connect operation.
      std::chrono::steady_clock::duration connect_timeout = std::chrono::seconds{10};

      /// Timeout of the resp3 handshake operation.
      std::chrono::steady_clock::duration resp3_handshake_timeout = std::chrono::seconds{2};

      /// Time interval of ping operations.
      std::chrono::steady_clock::duration ping_interval = std::chrono::seconds{1};
   };

   /// Constructor
   explicit connection(executor_type ex)
   : base_type{ex}
   , stream_{ex}
   {}

   explicit connection(boost::asio::io_context& ioc)
   : connection(ioc.get_executor())
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

   /** @brief Starts communication with the Redis server asynchronously.
    *
    *  This function performs the following steps
    *
    *  @li Resolves the Redis host as of `async_resolve` with the
    *  timeout passed in the base class `connection::timeouts::resolve_timeout`.
    *
    *  @li Connects to one of the endpoints returned by the resolve
    *  operation with the timeout passed in the base class
    *  `connection::timeouts::connect_timeout`.
    *
    *  @li Performs a RESP3 handshake by sending a
    *  [HELLO](https://redis.io/commands/hello/) command with protocol
    *  version 3 and the credentials contained in the
    *  `aedis::endpoint` object.  The timeout used is the one specified
    *  in `connection::timeouts::resp3_handshake_timeout`.
    *
    *  @li Erases any password that may be contained in
    *  `endpoint::password`.
    *
    *  @li Checks whether the server role corresponds to the one
    *  specifed in the `endpoint`. If `endpoint::role` is left empty,
    *  no check is performed. If the role role is different than the
    *  expected `async_run` will complete with
    *  `error::unexpected_server_role`.
    *
    *  @li Starts healthy checks with a timeout twice the value of
    *  `connection::timeouts::ping_interval`. If no data is received during that
    *  time interval `connection::async_run` completes with
    *  `error::idle_timeout`.
    *
    *  @li Starts the healthy check operation that sends the
    *  [PING](https://redis.io/commands/ping/) to Redis with a
    *  frequency equal to `connection::timeouts::ping_interval`.
    *
    *  @li Starts reading from the socket and executes all requests
    *  that have been started prior to this function call.
    *
    *  @param ep Redis endpoint.
    *  @param ts Timeouts used by the operations.
    *  @param token Completion token.
    *
    *  The completion token must have the following signature
    *
    *  @code
    *  void f(boost::system::error_code);
    *  @endcode
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

   /** @brief Executes a command on the Redis server asynchronously.
    *
    *  This function will send a request to the Redis server and
    *  complete when the response arrives. If the request contains
    *  only commands that don't expect a response, the completion
    *  occurs after it has been written to the underlying stream.
    *  Multiple concurrent calls to this function will be
    *  automatically queued by the implementation.
    *
    *  @param req Request object.
    *  @param adapter Response adapter.
    *  @param token Asio completion token.
    *
    *  For an example see echo_server.cpp. The completion token must
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
    *  will hang and eventually timeout.
    *
    *  @param adapter The response adapter.
    *  @param token The Asio completion token.
    *
    *  For an example see subscriber.cpp. The completion token must
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
   auto async_receive_push(
      Adapter adapter = adapt(),
      CompletionToken token = CompletionToken{})
   {
      return base_type::async_receive_push(adapter, std::move(token));
   }

   /** @brief Cancel operations.
    *
    *  @li `operation::exec`: Cancels operations started with
    *  `async_exec`. Has precedence over
    *  `request::config::close_on_connection_lost`
    *  @li operation::run: Cancels the `async_run` operation. Notice
    *  that the preferred way to close a connection is to send a
    *  [QUIT](https://redis.io/commands/quit/) command to the server.
    *  An unresponsive Redis server will also cause the idle-checks to
    *  timeout and lead to `connection::async_run` completing with
    *  `error::idle_timeout`.  Calling `cancel(operation::run)`
    *  directly should be seen as the last option.
    *  @li operation::receive_push: Cancels any ongoing callto
    *  `async_receive_push`.
    *
    *  @param op: The operation to be cancelled.
    *  @returns The number of operations that have been canceled.
    */
   auto cancel(operation op) -> std::size_t
      { return base_type::cancel(op); }

private:
   using this_type = connection<next_layer_type>;

   template <class, class> friend class detail::connection_base;
   template <class, class> friend struct detail::exec_read_op;
   template <class, class> friend struct detail::exec_op;
   template <class, class> friend struct detail::receive_push_op;
   template <class> friend struct detail::check_idle_op;
   template <class> friend struct detail::reader_op;
   template <class> friend struct detail::writer_op;
   template <class, class> friend struct detail::connect_with_timeout_op;
   template <class, class> friend struct detail::run_op;
   template <class> friend struct detail::ping_op;

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
         >(detail::connect_with_timeout_op<this_type, Timer>{this, &endpoints, ts, &timer},
               token, stream_);
   }

   void close() { stream_.close(); }
   auto is_open() const noexcept { return stream_.is_open(); }
   auto& lowest_layer() noexcept { return stream_.lowest_layer(); }

   AsyncReadWriteStream stream_;
};

} // aedis

#endif // AEDIS_CONNECTION_HPP
