/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_SSL_CONNECTION_HPP
#define BOOST_REDIS_SSL_CONNECTION_HPP

#include <boost/redis/detail/connection_base.hpp>
#include <boost/redis/detail/runner.hpp>
#include <boost/redis/ssl/detail/handshaker.hpp>
#include <boost/redis/detail/reconnection.hpp>
#include <boost/redis/response.hpp>
#include <boost/redis/logger.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <chrono>
#include <memory>

namespace boost::redis::ssl {

template <class>
class basic_connection;

/** \brief A SSL connection to the Redis server.
 *  \ingroup high-level-api
 *
 *  This class keeps a healthy connection to the Redis instance where
 *  commands can be sent at any time. For more details, please see the
 *  documentation of each individual function.
 *
 *  @tparam Socket The socket type e.g. asio::ip::tcp::socket.
 *
 */
template <class Socket>
class basic_connection<asio::ssl::stream<Socket>> :
   private redis::detail::connection_base<
      typename asio::ssl::stream<Socket>::executor_type,
      basic_connection<asio::ssl::stream<Socket>>> {
public:
   /// Type of the next layer
   using next_layer_type = asio::ssl::stream<Socket>;

   /// Executor type.
   using executor_type = typename next_layer_type::executor_type;

   /// Rebinds the socket type to another executor.
   template <class Executor1>
   struct rebind_executor
   {
      /// The socket type when rebound to the specified executor.
      using other = basic_connection<asio::ssl::stream<typename Socket::template rebind_executor<Executor1>::other>>;
   };

   using base_type = redis::detail::connection_base<executor_type, basic_connection<asio::ssl::stream<Socket>>>;

   /// Constructor
   explicit
   basic_connection(executor_type ex, asio::ssl::context& ctx)
   : base_type{ex}
   , ctx_{&ctx}
   , reconn_{ex}
   , runner_{ex, {}}
   , stream_{std::make_unique<next_layer_type>(ex, ctx)}
   { }

   /// Constructor
   explicit
   basic_connection(asio::io_context& ioc, asio::ssl::context& ctx)
   : basic_connection(ioc.get_executor(), ctx)
   { }

   /// Returns the associated executor.
   auto get_executor() {return stream_->get_executor();}

   /// Reset the underlying stream.
   void reset_stream()
   {
      if (stream_->next_layer().is_open()) {
         stream_->next_layer().close();
         stream_ = std::make_unique<next_layer_type>(stream_->get_executor(), *ctx_);
      }
   }

   /// Returns a reference to the next layer.
   auto& next_layer() noexcept { return *stream_; }

   /// Returns a const reference to the next layer.
   auto const& next_layer() const noexcept { return *stream_; }

   /** @brief Establishes a connection with the Redis server asynchronously.
    *
    *  See redis::connection::async_run for more information.
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

   /** @brief Executes a command on the Redis server asynchronously.
    *
    *  See redis::connection::async_exec for more information.
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
    *  See redis::connection::async_receive for detailed information.
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
    *  See redis::connection::cancel for more information.
    */
   auto cancel(operation op = operation::all) -> std::size_t
   {
      reconn_.cancel(op);
      runner_.cancel(op);
      return base_type::cancel(op);
   }

   auto& lowest_layer() noexcept { return stream_->lowest_layer(); }

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
   using runner_type = redis::detail::runner<executor_type, detail::handshaker>;
   using reconnection_type = redis::detail::basic_reconnection<executor_type>;
   using this_type = basic_connection<next_layer_type>;

   template <class Logger, class CompletionToken>
   auto async_run_one(Logger l, CompletionToken token)
      { return runner_.async_run(*this, l, std::move(token)); }

   template <class Logger, class CompletionToken>
   auto async_run_impl(Logger l, CompletionToken token)
      { return base_type::async_run_impl(l, std::move(token)); }

   template <class, class> friend class redis::detail::connection_base;
   template <class, class> friend class redis::detail::read_next_op;
   template <class, class> friend struct redis::detail::exec_op;
   template <class, class> friend struct redis::detail::receive_op;
   template <class, class> friend struct redis::detail::run_op;
   template <class, class> friend struct redis::detail::writer_op;
   template <class> friend struct redis::detail::reader_op;
   template <class> friend struct redis::detail::wait_receive_op;
   template <class, class, class> friend struct redis::detail::run_all_op;
   template <class, class, class> friend struct redis::detail::reconnection_op;

   auto is_open() const noexcept { return stream_->next_layer().is_open(); }

   void close()
      { reset_stream(); }

   asio::ssl::context* ctx_;
   reconnection_type reconn_;
   runner_type runner_;
   std::unique_ptr<next_layer_type> stream_;
};

/** \brief A connection that uses a boost::asio::ssl::stream<boost::asio::ip::tcp::socket>.
 *  \ingroup high-level-api
 */
using connection = basic_connection<asio::ssl::stream<asio::ip::tcp::socket>>;

} // boost::redis::ssl

#endif // BOOST_REDIS_SSL_CONNECTION_HPP
