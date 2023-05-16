/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONNECTION_HPP
#define BOOST_REDIS_CONNECTION_HPP

#include <boost/redis/connection_base.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/config.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/any_io_executor.hpp>

#include <chrono>
#include <memory>
#include <iostream>

namespace boost::redis {
namespace detail
{
template <class Connection, class Logger>
struct reconnection_op {
   Connection* conn_ = nullptr;
   Logger logger_;
   asio::coroutine coro_{};

   template <class Self>
   void operator()(Self& self, system::error_code ec = {})
   {
      BOOST_ASIO_CORO_REENTER (coro_) for (;;)
      {
         BOOST_ASIO_CORO_YIELD
         conn_->async_run_one(conn_->cfg_, logger_, std::move(self));
         conn_->cancel(operation::receive);
         logger_.on_connection_lost(ec);
         if (!conn_->will_reconnect() || is_cancelled(self)) {
            conn_->cancel(operation::reconnection);
            self.complete(!!ec ? ec : asio::error::operation_aborted);
            return;
         }

         conn_->timer_.expires_after(conn_->cfg_.reconnect_wait_interval);
         BOOST_ASIO_CORO_YIELD
         conn_->timer_.async_wait(std::move(self));
         BOOST_REDIS_CHECK_OP0(;)
         if (!conn_->will_reconnect()) {
            self.complete(asio::error::operation_aborted);
            return;
         }
         conn_->reset_stream();
      }
   }
};
} // detail

/** @brief A SSL connection to the Redis server.
 *  @ingroup high-level-api
 *
 *  This class keeps a healthy connection to the Redis instance where
 *  commands can be sent at any time. For more details, please see the
 *  documentation of each individual function.
 *
 *  @tparam Socket The socket type e.g. asio::ip::tcp::socket.
 *
 */
template <class Executor>
class basic_connection : public connection_base<Executor> {
public:
   using base_type = connection_base<Executor>;
   using this_type = basic_connection<Executor>;

   /// Executor type.
   using executor_type = Executor;

   /// Rebinds the socket type to another executor.
   template <class Executor1>
   struct rebind_executor
   {
      /// The connection type when rebound to the specified executor.
      using other = basic_connection<Executor1>;
   };

   /// Contructs from an executor.
   explicit
   basic_connection(executor_type ex, asio::ssl::context::method method = asio::ssl::context::tls_client)
   : base_type{ex, method}
   , timer_{ex}
   { }

   /// Contructs from a context.
   explicit
   basic_connection(asio::io_context& ioc, asio::ssl::context::method method = asio::ssl::context::tls_client)
   : basic_connection(ioc.get_executor(), method)
   { }

   /** @brief High-level connection to Redis.
    *
    *  This connection class adds reconnection functionality to
    *  `boost::redis::connection_base::async_run_one`.  When a
    *  connection is lost for any reason, a new one is stablished
    *  automatically. To disable reconnection call
    *  `boost::redis::connection::cancel(operation::reconnection)`.
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
    *  * This function will complete only if reconnection was disabled
    *    and the connection is lost.
    *
    *  For example on how to call this function refer to
    *  cpp20_intro.cpp or any other example.
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
      cfg_ = cfg;
      l.set_prefix(cfg_.log_prefix);
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(detail::reconnection_op<this_type, Logger>{this, l}, token, timer_);
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
   void cancel(operation op = operation::all) override
   {
      switch (op) {
         case operation::reconnection:
         case operation::all:
            cfg_.reconnect_wait_interval = std::chrono::seconds::zero();
            timer_.cancel();
            break;
         default: /* ignore */;
      }

      base_type::cancel(op);
   }

   /// Returns true if the connection was canceled.
   bool will_reconnect() const noexcept
      { return cfg_.reconnect_wait_interval != std::chrono::seconds::zero();}

private:
   config cfg_;

   using timer_type =
      asio::basic_waitable_timer<
         std::chrono::steady_clock,
         asio::wait_traits<std::chrono::steady_clock>,
         Executor>;

   template <class, class> friend struct detail::reconnection_op;

   timer_type timer_;
};

/** \brief A connection that uses the asio::any_io_executor.
 *  \ingroup high-level-api
 */
using connection = basic_connection<asio::any_io_executor>;

} // boost::redis

#endif // BOOST_REDIS_CONNECTION_HPP
