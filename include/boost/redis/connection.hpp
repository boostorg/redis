/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONNECTION_HPP
#define BOOST_REDIS_CONNECTION_HPP

#include <boost/redis/detail/connection_base.hpp>
#include <boost/redis/detail/runner.hpp>
#include <boost/redis/detail/reconnection.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/response.hpp>
#include <boost/asio/io_context.hpp>

#include <chrono>
#include <memory>

namespace boost::redis {

namespace detail
{

template <class Executor>
class dummy_handshaker {
public:
   dummy_handshaker(Executor) {}

   template <class Stream, class CompletionToken>
   auto async_handshake(Stream&, CompletionToken&& token)
      { return asio::post(std::move(token)); }

   void set_config(config const&) {}

   std::size_t cancel(operation) { return 0;}

   constexpr bool is_dummy() const noexcept {return true;}
};

}

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
   , reconn_{ex}
   , runner_{ex, {}}
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
         system::error_code ec;
         stream_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
         stream_.close(ec);
      }
   }

   /// Returns a reference to the next layer.
   auto next_layer() noexcept -> auto& { return stream_; }

   /// Returns a const reference to the next layer.
   auto next_layer() const noexcept -> auto const& { return stream_; }

   /** @brief Starts underlying connection operations.
    *
    *  In more detail, this function will
    *
    *  1. Resolve the address passed on `boost::redis::config::addr`.
    *  2. Connect to one of the results obtained in the resolve operation.
    *  3. Send a [HELLO](https://redis.io/commands/hello/) command where each of its parameters are read from `cfg`.
    *  4. Start a health-check operation where ping commands are sent
    *     at intervals specified in
    *     `boost::redis::config::health_check_interval`.  The message passed to
    *     `PING` will be `boost::redis::config::health_check_id`.  Passing a
    *     timeout with value zero will disable health-checks.  If the Redis
    *     server does not respond to a health-check within two times the value
    *     specified here, it will be considered unresponsive and the connection
    *     will be closed and a new connection will be stablished.
    *  5. Starts read and write operations with the Redis
    *  server. More specifically it will trigger the write of all
    *  requests i.e. calls to `async_exec` that happened prior to this
    *  call.
    *
    *  When a connection is lost for any reason, a new one is stablished automatically. To disable
    *  reconnection call `boost::redis::connection::cancel(operation::reconnection)`.
    *
    *  @param cfg Configuration paramters.
    *  @param l Logger object. The interface expected is specified in the class `boost::redis::logger`.
    *  @param token Completion token.
    *
    *  The completion token must have the following signature
    *
    *  @code
    *  void f(system::error_code);
    *  @endcode
    *
    *  @remarks
    *
    *  * This function will complete only if reconnection was disabled and the connection is lost.
    *
    *  For example on how to call this function refer to cpp20_intro.cpp or any other example.
    */
   template <
      class Logger = logger,
      class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto
   async_run(
      config const& cfg = {},
      Logger l = Logger{},
      CompletionToken token = CompletionToken{})
   {
      reconn_.set_wait_interval(cfg.reconnect_wait_interval);
      runner_.set_config(cfg);
      l.set_prefix(runner_.get_config().log_prefix);
      return reconn_.async_run(*this, l, std::move(token));
   }

   /** @brief Executes commands on the Redis server asynchronously.
    *
    *  This function sends a request to the Redis server and
    *  waits for the responses to each individual command in the
    *  request to arrive. If the request
    *  contains only commands that don't expect a response, the
    *  completion occurs after it has been written to the underlying
    *  stream.  Multiple concurrent calls to this function will be
    *  automatically queued by the implementation.
    *
    *  @param req Request object.
    *  @param resp Response object.
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
      Response& resp = ignore,
      CompletionToken token = CompletionToken{})
   {
      return base_type::async_exec(req, resp, std::move(token));
   }

   /** @brief Receives server side pushes asynchronously.
    *
    *  When pushes arrive and there is no `async_receive` operation in
    *  progress, pushed data, requests, and responses will be paused
    *  until `async_receive` is called again.  Apps will usually want to
    *  call `async_receive` in a loop. 
    *
    *  To cancel an ongoing receive operation apps should call
    *  `connection::cancel(operation::receive)`.
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
    *  @li operation::run: Cancels the `async_run` operation.
    *  @li operation::receive: Cancels any ongoing calls to `async_receive`.
    *  @li operation::all: Cancels all operations listed above.
    *
    *  @param op: The operation to be cancelled.
    *  @returns The number of operations that have been canceled.
    */
   auto cancel(operation op = operation::all) -> std::size_t
   {
      reconn_.cancel(op);
      runner_.cancel(op);
      return base_type::cancel(op);
   }

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

   /// Returns true if the connection was canceled.
   bool is_cancelled() const noexcept
      { return reconn_.is_cancelled();}

private:
   using runner_type = detail::runner<executor_type, detail::dummy_handshaker>;
   using reconnection_type = detail::basic_reconnection<executor_type>;
   using this_type = basic_connection<next_layer_type>;

   template <class, class> friend class detail::connection_base;
   template <class, class> friend class detail::read_next_op;
   template <class, class> friend struct detail::exec_op;
   template <class, class> friend struct detail::receive_op;
   template <class> friend struct detail::reader_op;
   template <class, class> friend struct detail::writer_op;
   template <class, class> friend struct detail::run_op;
   template <class> friend struct detail::wait_receive_op;
   template <class, class, class> friend struct detail::run_all_op;
   template <class, class, class> friend struct detail::reconnection_op;

   template <class Logger, class CompletionToken>
   auto async_run_one(Logger l, CompletionToken token)
      { return runner_.async_run(*this, l, std::move(token)); }

   template <class Logger, class CompletionToken>
   auto async_run_impl(Logger l, CompletionToken token)
      { return base_type::async_run_impl(l, std::move(token)); }

   void close()
      { reset_stream(); }

   auto is_open() const noexcept { return stream_.is_open(); }
   auto lowest_layer() noexcept -> auto& { return stream_.lowest_layer(); }

   reconnection_type reconn_;
   runner_type runner_;
   Socket stream_;
};

/** \brief A connection that uses a asio::ip::tcp::socket.
 *  \ingroup high-level-api
 */
using connection = basic_connection<asio::ip::tcp::socket>;

} // boost::redis

#endif // BOOST_REDIS_CONNECTION_HPP
