/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_HEALTH_CHECKER_HPP
#define BOOST_REDIS_HEALTH_CHECKER_HPP

// Has to included before promise.hpp to build on msvc.
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>
#include <boost/redis/operation.hpp>
#include <boost/redis/detail/helper.hpp>
#include <boost/redis/config.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <memory>
#include <chrono>

namespace boost::redis::detail {

template <class HealthChecker, class Connection>
class ping_op {
public:
   HealthChecker* checker_ = nullptr;
   Connection* conn_ = nullptr;
   asio::coroutine coro_{};

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro_) for (;;)
      {
         if (checker_->checker_has_exited_) {
            self.complete({});
            return;
         }

         BOOST_ASIO_CORO_YIELD
         conn_->async_exec(checker_->req_, checker_->resp_, std::move(self));
         BOOST_REDIS_CHECK_OP0(checker_->wait_timer_.cancel();)

         // Wait before pinging again.
         checker_->ping_timer_.expires_after(checker_->ping_interval_);
         BOOST_ASIO_CORO_YIELD
         checker_->ping_timer_.async_wait(std::move(self));
         BOOST_REDIS_CHECK_OP0(;)
      }
   }
};

template <class HealthChecker, class Connection>
class check_timeout_op {
public:
   HealthChecker* checker_ = nullptr;
   Connection* conn_ = nullptr;
   asio::coroutine coro_{};

   template <class Self>
   void operator()(Self& self, system::error_code ec = {})
   {
      BOOST_ASIO_CORO_REENTER (coro_) for (;;)
      {
         checker_->wait_timer_.expires_after(2 * checker_->ping_interval_);
         BOOST_ASIO_CORO_YIELD
         checker_->wait_timer_.async_wait(std::move(self));
         BOOST_REDIS_CHECK_OP0(;)

         if (checker_->resp_.has_error()) {
            self.complete({});
            return;
         }

         if (checker_->resp_.value().empty()) {
            checker_->ping_timer_.cancel();
            conn_->cancel(operation::run);
            checker_->checker_has_exited_ = true;
            self.complete(error::pong_timeout);
            return;
         }

         if (checker_->resp_.has_value()) {
            checker_->resp_.value().clear();
         }
      }
   }
};

template <class HealthChecker, class Connection>
class check_health_op {
public:
   HealthChecker* checker_ = nullptr;
   Connection* conn_ = nullptr;
   asio::coroutine coro_{};

   template <class Self>
   void
   operator()(
         Self& self,
         std::array<std::size_t, 2> order = {},
         system::error_code ec1 = {},
         system::error_code ec2 = {})
   {
      BOOST_ASIO_CORO_REENTER (coro_)
      {
         if (checker_->ping_interval_ == std::chrono::seconds::zero()) {
            BOOST_ASIO_CORO_YIELD
            asio::post(std::move(self));
            self.complete({});
            return;
         }

         BOOST_ASIO_CORO_YIELD
         asio::experimental::make_parallel_group(
            [this](auto token) { return checker_->async_ping(*conn_, token); },
            [this](auto token) { return checker_->async_check_timeout(*conn_, token);}
         ).async_wait(
            asio::experimental::wait_for_one(),
            std::move(self));

         if (is_cancelled(self)) {
            self.complete(asio::error::operation_aborted);
            return;
         }

         switch (order[0]) {
            case 0: self.complete(ec1); return;
            case 1: self.complete(ec2); return;
            default: BOOST_ASSERT(false);
         }
      }
   }
};

template <class Executor>
class health_checker {
private:
   using timer_type =
      asio::basic_waitable_timer<
         std::chrono::steady_clock,
         asio::wait_traits<std::chrono::steady_clock>,
         Executor>;

public:
   health_checker(Executor ex)
   : ping_timer_{ex}
   , wait_timer_{ex}
   {
      req_.push("PING", "Boost.Redis");
   }

   void set_config(config const& cfg)
   {
      req_.clear();
      req_.push("PING", cfg.health_check_id);
      ping_interval_ = cfg.health_check_interval;
   }

   template <
      class Connection,
      class CompletionToken = asio::default_completion_token_t<Executor>
   >
   auto async_check_health(Connection& conn, CompletionToken token = CompletionToken{})
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(check_health_op<health_checker, Connection>{this, &conn}, token, conn);
   }

   std::size_t cancel(operation op)
   {
      switch (op) {
         case operation::health_check:
         case operation::all:
            ping_timer_.cancel();
            wait_timer_.cancel();
            break;
         default: /* ignore */;
      }

      return 0;
   }

private:
   template <class Connection, class CompletionToken>
   auto async_ping(Connection& conn, CompletionToken token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(ping_op<health_checker, Connection>{this, &conn}, token, conn, ping_timer_);
   }

   template <class Connection, class CompletionToken>
   auto async_check_timeout(Connection& conn, CompletionToken token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(check_timeout_op<health_checker, Connection>{this, &conn}, token, conn, wait_timer_);
   }

   template <class, class> friend class ping_op;
   template <class, class> friend class check_timeout_op;
   template <class, class> friend class check_health_op;

   timer_type ping_timer_;
   timer_type wait_timer_;
   redis::request req_;
   redis::generic_response resp_;
   std::chrono::steady_clock::duration ping_interval_ = std::chrono::seconds{5};
   bool checker_has_exited_ = false;
};

} // boost::redis::detail

#endif // BOOST_REDIS_HEALTH_CHECKER_HPP
