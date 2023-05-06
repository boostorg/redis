/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RECONNECTION_HPP
#define BOOST_REDIS_RECONNECTION_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/logger.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/any_io_executor.hpp>

namespace boost::redis::detail
{

template <class Reconnector, class Connection, class Logger>
struct reconnection_op {
   Reconnector* reconn_ = nullptr;
   Connection* conn_ = nullptr;
   Logger logger_;
   asio::coroutine coro_{};

   template <class Self>
   void operator()(Self& self, system::error_code ec = {})
   {
      BOOST_ASIO_CORO_REENTER (coro_) for (;;)
      {
         BOOST_ASIO_CORO_YIELD
         conn_->async_run_one(logger_, std::move(self));
         conn_->reset_stream();
         conn_->cancel(operation::receive);
         logger_.on_connection_lost(ec);
         if (reconn_->is_cancelled() || is_cancelled(self)) {
            reconn_->cancel(operation::reconnection);
            self.complete(!!ec ? ec : asio::error::operation_aborted);
            return;
         }

         reconn_->timer_.expires_after(reconn_->wait_interval_);
         BOOST_ASIO_CORO_YIELD
         reconn_->timer_.async_wait(std::move(self));
         BOOST_REDIS_CHECK_OP0(;)
         if (reconn_->is_cancelled()) {
            self.complete(asio::error::operation_aborted);
            return;
         }
      }
   }
};

// NOTE: wait_interval could be an async_run parameter.

template <class Executor>
class basic_reconnection {
public:
   /// Executor type.
   using executor_type = Executor;

   basic_reconnection(Executor ex)
   : timer_{ex}
   , is_cancelled_{false}
   {}

   basic_reconnection(asio::io_context& ioc, std::chrono::steady_clock::duration wait_interval)
   : basic_reconnection{ioc.get_executor(), wait_interval}
   {}

   /// Rebinds to a new executor type.
   template <class Executor1>
   struct rebind_executor
   {
      using other = basic_reconnection<Executor1>;
   };

   template <
      class Connection,
      class Logger = logger,
      class CompletionToken = asio::default_completion_token_t<executor_type>
   >
   auto
   async_run(
      Connection& conn,
      Logger l = Logger{},
      CompletionToken token = CompletionToken{})
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(detail::reconnection_op<basic_reconnection, Connection, Logger>{this, &conn, l}, token, conn);
   }

   void set_wait_interval(std::chrono::steady_clock::duration wait_interval)
   {
      wait_interval_ = wait_interval;
   }

   std::size_t cancel(operation op)
   {
      switch (op) {
         case operation::reconnection:
         case operation::all:
            is_cancelled_ = true;
            timer_.cancel();
            break;
         default: /* ignore */;
      }

      return 0U;
   }

   bool is_cancelled() const noexcept {return is_cancelled_;}
   void reset() noexcept {is_cancelled_ = false;}

private:
   using timer_type =
      asio::basic_waitable_timer<
         std::chrono::steady_clock,
         asio::wait_traits<std::chrono::steady_clock>,
         Executor>;

   template <class, class, class> friend struct detail::reconnection_op;

   timer_type timer_;
   std::chrono::steady_clock::duration wait_interval_ = std::chrono::seconds{1};
   bool is_cancelled_;
};

using reconnection = basic_reconnection<asio::any_io_executor>;

} // boost::redis

#endif // BOOST_REDIS_RECONNECTION_HPP
