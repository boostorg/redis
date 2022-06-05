/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_GENERIC_CONNECTION_HPP
#define AEDIS_GENERIC_CONNECTION_HPP

#include <vector>
#include <queue>
#include <limits>
#include <chrono>
#include <memory>
#include <type_traits>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

#include <aedis/resp3/request.hpp>
#include <aedis/generic/adapt.hpp>
#include <aedis/generic/detail/connection_ops.hpp>

namespace aedis {
namespace generic {

/** \brief A high level Redis connection.
 *  \ingroup any
 *
 *  This class keeps a connection open to the Redis server where
 *  commands can be sent at any time. For more details, please see the
 *  documentation of each individual function.
 *
 *  https://redis.io/docs/reference/sentinel-clients
 */
template <class Command, class AsyncReadWriteStream = boost::asio::ip::tcp::socket>
class connection {
public:
   /// Executor type.
   using executor_type = typename AsyncReadWriteStream::executor_type;

   /// Type of the last layer
   using next_layer_type = AsyncReadWriteStream;

   /// Type of requests used by the connection.
   using request_type = resp3::request<Command>;

   using default_completion_token_type = boost::asio::default_completion_token_t<executor_type>;

   /** @brief Configuration parameters.
    */
   struct config {
      /// Timeout of the \c async_resolve operation.
      std::chrono::milliseconds resolve_timeout = std::chrono::seconds{5};

      /// Timeout of the \c async_connect operation.
      std::chrono::milliseconds connect_timeout = std::chrono::seconds{5};

      /// Timeout of the \c async_read operation.
      std::chrono::milliseconds read_timeout = std::chrono::seconds{5};

      /// Timeout of the \c async_write operation.
      std::chrono::milliseconds write_timeout = std::chrono::seconds{5};

      /// Time after which a ping is sent if no data is received.
      std::chrono::milliseconds ping_interval = std::chrono::seconds{5};

      /// The maximum size allwed in a read operation.
      std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)();
   };

   /** \brief Constructor.
    *
    *  \param ex The executor.
    *  \param cfg Configuration parameters.
    */
   connection(boost::asio::any_io_executor ex, config cfg = config{})
   : resv_{ex}
   , wait_write_timer_{ex}
   , ping_timer_{ex}
   , write_timer_{ex}
   , wait_read_timer_{ex}
   , wait_push_timer_{ex}
   , check_idle_timer_{ex}
   , cfg_{cfg}
   , last_data_{std::chrono::time_point<std::chrono::steady_clock>::min()}
   {
      wait_push_timer_.expires_at(std::chrono::steady_clock::time_point::max());
      wait_write_timer_.expires_at(std::chrono::steady_clock::time_point::max());
   }

   connection(boost::asio::io_context& ioc, config cfg = config{})
   : connection(ioc.get_executor(), cfg)
   { }

   /// Returns the executor.
   auto get_executor() {return resv_.get_executor();}

   /** @brief Starts communication with the Redis server asynchronously.
    *
    *  This function performs the following steps
    *
    *  @li Resolves the Redis host as of \c async_resolve with the
    *  timeout passed in connection::config::resolve_timeout.
    *
    *  @li Connects to one of the endpoints returned by the resolve
    *  operation with the timeout passed in connection::config::connect_timeout.
    *
    *  @li Starts the \c async_read operation that keeps reading incoming
    *  responses. Each individual read uses the timeout passed on
    *  connection::config::read_timeout. After each successful read it
    *  will call the read or push callback.
    *
    *  @li Starts the \c async_write operation that waits for new commands
    *  to be sent to Redis. Each individual write uses the timeout
    *  passed on connection::config::write_timeout. After a successful
    *  write it will call the write callback.
    *
    *  @li Starts the idle check operation with the timeout of twice
    *  the value of connection::config::ping_interval. If no data is
    *  received during that time interval \c async_run completes with
    *  generic::error::idle_timeout.
    *
    *  @li Starts the healthy check operation that sends
    *  redis::command::ping to Redis with a frequency equal to
    *  connection::config::ping_interval.
    *
    *  In addition to the callbacks mentioned above, the read
    *  operations will call the resp3 callback as soon a new chunks of
    *  data become available to the user.
    *
    *  It is safe to call \c async_run after it has returned.  In this
    *  case, any outstanding commands will be sent after the
    *  connection is restablished. If a disconnect occurs while the
    *  response to a request has not been received, the connection doesn't
    *  try to resend it to avoid resubmission.
    *
    *  Example:
    *
    *  @code
    *  awaitable<void> run_with_reconnect(std::shared_ptr<connection_type> db)
    *  {
    *     auto ex = co_await this_coro::executor;
    *     asio::steady_timer timer{ex};
    *  
    *     for (error_code ec;;) {
    *        co_await db->async_run("127.0.0.1", "6379", redirect_error(use_awaitable, ec));
    *        timer.expires_after(std::chrono::seconds{2});
    *        co_await timer.async_wait(redirect_error(use_awaitable, ec));
    *     }
    *  }
    *  @endcode
    *
    *  \param token The completion token.
    *
    *  The completion token must have the following signature
    *
    *  @code
    *  void f(boost::system::error_code);
    *  @endcode
    *
    *  \return This function returns only when there is an error.
    */
   template <class CompletionToken = default_completion_token_type>
   auto
   async_run(
      boost::string_view host = "127.0.0.1",
      boost::string_view port = "6379",
      CompletionToken token = CompletionToken{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::run_op<connection, Command>{this, host, port}, token, resv_);
   }

   /** @brief Asynchrnously schedules a command for execution.
    */
   template <
      class Adapter = detail::response_traits<void>::adapter_type,
      class CompletionToken = default_completion_token_type>
   auto async_exec(
      request_type const& req,
      Adapter adapter = generic::adapt(),
      CompletionToken token = CompletionToken{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t)
         >(detail::exec_op<connection, Adapter>{this, &req, adapter}, token, resv_);
   }

   /** @brief Receives events produced by the run operation.
    */
   template <
      class Adapter = detail::response_traits<void>::adapter_type,
      class CompletionToken = default_completion_token_type>
   auto
   async_read_push(
      Adapter adapter = generic::adapt(),
      CompletionToken token = CompletionToken{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t)
         >(detail::read_push_op<connection, Adapter, Command>{this, adapter}, token, resv_);
   }

   /** @brief Closes the connection with the database.
    *  
    *  The channels will be cancelled.
    */
   void close()
   {
      socket_->close();
      wait_read_timer_.expires_at(std::chrono::steady_clock::now());
      wait_push_timer_.expires_at(std::chrono::steady_clock::now());
      wait_write_timer_.expires_at(std::chrono::steady_clock::now());
      check_idle_timer_.expires_at(std::chrono::steady_clock::now());
      ping_timer_.cancel();
      for (auto& e: reqs_) {
         e->stop = true;
         e->timer.cancel_one();
      }

      reqs_ = {};
   }

private:
   using time_point_type = std::chrono::time_point<std::chrono::steady_clock>;

   template <class T, class U, class V> friend struct detail::read_push_op;
   template <class T, class U> friend struct detail::reader_op;
   template <class T, class U> friend struct detail::ping_op;
   template <class T, class U> friend struct detail::run_op;
   template <class T, class U> friend struct detail::exec_op;
   template <class T> friend struct detail::exec_internal_op;
   template <class T> friend struct detail::writer_op;
   template <class T> friend struct detail::write_with_timeout_op;
   template <class T> friend struct detail::connect_with_timeout_op;
   template <class T> friend struct detail::resolve_with_timeout_op;
   template <class T> friend struct detail::check_idle_op;
   template <class T> friend struct detail::read_write_check_ping_op;

   struct req_info {
      boost::asio::steady_timer timer;
      std::size_t n_cmds = 0;
      bool stop = false;
   };

   void add_request(request_type const& req)
   {
      BOOST_ASSERT(!req.payload().empty());
      auto const can_write = reqs_.empty();
      reqs_.push_back(make_req_info(req.commands().size()));
      n_cmds_next_ += req.commands().size();
      payload_next_ += req.payload();
      for (auto cmd : req.commands())
         cmds_.push(cmd.first);
      if (can_write) {
         BOOST_ASSERT(n_cmds_ == 0);
         wait_write_timer_.cancel_one();
      }
   }

   auto make_dynamic_buffer()
      { return boost::asio::dynamic_buffer(read_buffer_, cfg_.max_read_size); }

   template <class CompletionToken = default_completion_token_type>
   auto
   async_resolve_with_timeout(
      boost::string_view host,
      boost::string_view port,
      CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::resolve_with_timeout_op<connection>{this, host, port},
            token, resv_);
   }

   // Calls connection::async_connect with a timeout.
   template <class CompletionToken = default_completion_token_type>
   auto
   async_connect_with_timeout(
         CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::connect_with_timeout_op<connection>{this}, token, resv_);
   }

   // Loops on async_read_with_timeout described above.
   template <class CompletionToken = default_completion_token_type>
   auto
   reader(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::reader_op<connection, Command>{this}, token, resv_.get_executor());
   }

   // Write with a timeout.
   template <class CompletionToken = default_completion_token_type>
   auto
   async_write_with_timeout(
      CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t)
         >(detail::write_with_timeout_op<connection>{this}, token, resv_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   writer(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::writer_op<connection>{this}, token, resv_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   async_read_write_check_ping(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::read_write_check_ping_op<connection>{this}, token, resv_, resv_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   async_ping(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::ping_op<connection, Command>{this}, token, resv_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   async_idle_check(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::check_idle_op<connection>{this}, token, check_idle_timer_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto async_exec_internal(
      request_type const& req,
      CompletionToken token = CompletionToken{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::exec_internal_op<connection>{this, &req},
               token, resv_);
   }

   std::shared_ptr<req_info> make_req_info(std::size_t cmds)
   {
      if (pool_.empty())
         return std::make_shared<req_info>(
            boost::asio::steady_timer{resv_.get_executor()},
            cmds, false);

      auto ret = pool_.back();
      ret->n_cmds = cmds;
      ret->stop = false;
      pool_.pop_back();
      return ret;
   }

   void release_req_info(std::shared_ptr<req_info> info)
   {
      pool_.push_back(info);
   }

   // IO objects
   boost::asio::ip::tcp::resolver resv_;
   std::shared_ptr<AsyncReadWriteStream> socket_;
   boost::asio::steady_timer wait_write_timer_;
   boost::asio::steady_timer ping_timer_;
   boost::asio::steady_timer write_timer_;
   boost::asio::steady_timer wait_read_timer_;
   boost::asio::steady_timer wait_push_timer_;
   boost::asio::steady_timer check_idle_timer_;

   // Configuration parameters.
   config cfg_;

   // Buffer used by the read operations.
   std::string read_buffer_;

   std::size_t waiting_pushes_ = 0;
   std::size_t n_cmds_ = 0;
   std::size_t n_cmds_next_ = 0;
   std::string payload_;
   std::string payload_next_;
   std::deque<std::shared_ptr<req_info>> reqs_;
   std::queue<Command> cmds_; // TODO: Make this part of req_info.
   std::vector<std::shared_ptr<req_info>> pool_;

   // Last time we received data.
   time_point_type last_data_;

   // The result of async_resolve.
   boost::asio::ip::tcp::resolver::results_type endpoints_;

   request_type req_;
};

} // generic
} // aedis

#endif // AEDIS_GENERIC_CONNECTION_HPP
