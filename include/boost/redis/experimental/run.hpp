/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RUN_HPP
#define BOOST_REDIS_RUN_HPP

// Has to included before promise.hpp to build on msvc.
#include <tuple>
#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/experimental/use_promise.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <chrono>

namespace boost::redis::experimental {
namespace detail {

template <class Connection>
class check_health_op {
private:
   using executor_type = typename Connection::executor_type;

   struct state {
      using clock_type = std::chrono::steady_clock;
      using clock_traits_type = asio::wait_traits<clock_type>;
      using timer_type = asio::basic_waitable_timer<clock_type, clock_traits_type, executor_type>;
      using promise_type = asio::experimental::promise<void(system::error_code, std::size_t), executor_type>;

      timer_type timer_;
      request req_;
      generic_response resp_;
      std::optional<promise_type> prom_;
      std::chrono::steady_clock::duration interval_;

      state(
         executor_type ex,
         std::string const& msg,
         std::chrono::steady_clock::duration interval)
      : timer_{ex}
      , interval_{interval}
      {
         req_.push("PING", msg);
      }

      void reset()
      {
         resp_.value().clear();
         prom_.reset();
      }
   };

   Connection* conn_ = nullptr;
   std::shared_ptr<state> state_ = nullptr;
   asio::coroutine coro_{};

public:
   check_health_op(
      Connection& conn,
      std::string const& msg,
      std::chrono::steady_clock::duration interval)
   : conn_{&conn}
   , state_{std::make_shared<state>(conn.get_executor(), msg, interval)}
   { }

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro_) for (;;)
      {
         state_->prom_.emplace(conn_->async_exec(state_->req_, state_->resp_, asio::experimental::use_promise));

         state_->timer_.expires_after(state_->interval_);
         BOOST_ASIO_CORO_YIELD
         state_->timer_.async_wait(std::move(self));
         if (ec || is_cancelled(self) || state_->resp_.value().empty()) {
            conn_->cancel(operation::run);
            BOOST_ASIO_CORO_YIELD
            std::move(*state_->prom_)(std::move(self));
            self.complete({});
            return;
         }

         state_->reset();
      }
   }
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
   return asio::async_compose
      < CompletionToken
      , void(system::error_code)
      >(detail::check_health_op<Connection>{conn, msg, interval}, token, conn);
}

} // boost::redis::experimental

#endif // BOOST_REDIS_RUN_HPP
