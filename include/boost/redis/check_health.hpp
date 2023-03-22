/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CHECK_HEALTH_HPP
#define BOOST_REDIS_CHECK_HEALTH_HPP

// Has to included before promise.hpp to build on msvc.
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>
#include <boost/redis/operation.hpp>
#include <boost/redis/detail/read_ops.hpp>
#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/experimental/use_promise.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/consign.hpp>
#include <memory>
#include <chrono>
#include <optional>

namespace boost::redis {
namespace detail {

template <class HealthChecker, class Connection>
class check_health_op {
public:
   HealthChecker* checker = nullptr;
   Connection* conn = nullptr;
   asio::coroutine coro_{};

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro_) for (;;)
      {
         checker->prom_.emplace(conn->async_exec(checker->req_, checker->resp_, asio::experimental::use_promise));

         checker->timer_.expires_after(checker->timeout_);
         BOOST_ASIO_CORO_YIELD
         checker->timer_.async_wait(std::move(self));
         if (ec || is_cancelled(self) || checker->resp_.value().empty()) {
            conn->cancel(operation::run);
            BOOST_ASIO_CORO_YIELD
            std::move(*checker->prom_)(std::move(self));
            self.complete({});
            return;
         }

         checker->reset();
      }
   }
};

template <class Executor>
class health_checker {
private:
   using promise_type = asio::experimental::promise<void(system::error_code, std::size_t), Executor>;
   using timer_type =
      asio::basic_waitable_timer<
         std::chrono::steady_clock,
         asio::wait_traits<std::chrono::steady_clock>,
         Executor>;

public:
   health_checker(
      Executor ex,
      std::string const& msg,
      std::chrono::steady_clock::duration interval)
   : timer_{ex}
   , timeout_{interval}
   {
      req_.push("PING", msg);
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

   void reset()
   {
      resp_.value().clear();
      prom_.reset();
   }

   void cancel()
   {
      timer_.cancel();
      if (prom_)
         prom_.cancel();
   }

private:
   template <class, class> friend class check_health_op;
   timer_type timer_;
   std::optional<promise_type> prom_;
   redis::request req_;
   redis::generic_response resp_;
   std::chrono::steady_clock::duration timeout_;
};

} // detail

/** @brief Checks Redis health asynchronously
 *  @ingroup high-level-api
 *
 *  This function will ping the Redis server periodically until a ping
 *  timesout or an error occurs. On timeout this function will
 *  complete with success.
 *
 *  @param conn A connection to the Redis server.
 *  @param msg The message to be sent with the [PING](https://redis.io/commands/ping/) command. Seting a proper and unique id will help users identify which connections are active.
 *  @param interval Ping interval.
 *  @param token The completion token
 *
 *  The completion token must have the following signature
 *
 *  @code
 *  void f(system::error_code);
 *  @endcode
 */
template <
   class Connection,
   class CompletionToken = asio::default_completion_token_t<typename Connection::executor_type>
>
auto
async_check_health(
   Connection& conn,
   std::string const& msg = "Boost.Redis",
   std::chrono::steady_clock::duration interval = std::chrono::seconds{2},
   CompletionToken token = CompletionToken{})
{
   using executor_type = typename Connection::executor_type;
   using health_checker_type = detail::health_checker<executor_type>;
   auto checker = std::make_shared<health_checker_type>(conn.get_executor(), msg, interval);
   return checker->async_check_health(conn, asio::consign(std::move(token), checker));
}

} // boost::redis

#endif // BOOST_REDIS_CHECK_HEALTH_HPP
