/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONNECTOR_HPP
#define BOOST_REDIS_CONNECTOR_HPP

#include <boost/redis/detail/runner.hpp>
#include <boost/redis/check_health.hpp>
#include <boost/redis/response.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <string>
#include <chrono>

namespace boost::redis::experimental
{

struct connect_config {
   address addr = address{"127.0.0.1", "6379"};
   std::string username;
   std::string password;
   std::string clientname = "Boost.Redis";
   std::string health_check_id = "Boost.Redis";
   std::chrono::steady_clock::duration resolve_timeout = std::chrono::seconds{10};
   std::chrono::steady_clock::duration connect_timeout = std::chrono::seconds{10};
   std::chrono::steady_clock::duration health_check_timeout = std::chrono::seconds{2};
   std::chrono::steady_clock::duration reconnect_wait_interval = std::chrono::seconds{1};
};

namespace detail
{

template <class Connector, class Connection>
struct hello_op {
   Connector* ctor_ = nullptr;
   Connection* conn_ = nullptr;
   asio::coroutine coro_{};

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro_)
      {
         ctor_->req_hello_.clear();
         ctor_->resp_hello_.value().clear();
         ctor_->add_hello();

         BOOST_ASIO_CORO_YIELD
         conn_->async_exec(ctor_->req_hello_, ctor_->resp_hello_, std::move(self));

         ctor_->logger_.on_hello(ec);

         BOOST_REDIS_CHECK_OP0(conn_->cancel(redis::operation::run);)

         if (ctor_->resp_hello_.has_error()) {
            conn_->cancel(redis::operation::run);
            switch (ctor_->resp_hello_.error().data_type) {
               case resp3::type::simple_error:
               self.complete(error::resp3_simple_error);
               break;

               case resp3::type::blob_error:
               self.complete(error::resp3_blob_error);
               break;

               default: BOOST_ASSERT_MSG(false, "Unexpects error data type.");
            }
         } else {
            self.complete({});
         }
      }
   }
};

template <class Connector, class Connection>
struct run_check_exec_op {
   Connector* ctor_ = nullptr;
   Connection* conn_ = nullptr;
   asio::coroutine coro_{};

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , system::error_code ec1 = {}
                  , system::error_code ec2 = {}
                  , std::size_t = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro_)
      {
         BOOST_ASIO_CORO_YIELD
         asio::experimental::make_parallel_group(
            [this](auto token) { return ctor_->async_run_check(*conn_, token); },
            [this](auto token) { return ctor_->async_hello(*conn_, token); }
         ).async_wait(
            asio::experimental::wait_for_all(),
            std::move(self)
         );

         if (is_cancelled(self)) {
            self.complete(asio::error::operation_aborted);
            return;
         }

         // TODO: Which op should we use to complete?
         switch (order[0]) {
            case 0: self.complete(ec1); break;
            case 1: self.complete(ec2); break;
            default: BOOST_ASSERT(false);
         }
      }
   }
};

template <class Connector, class Connection>
struct run_check_op {
   Connector* ctor_ = nullptr;
   Connection* conn_ = nullptr;
   asio::coroutine coro_{};

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , system::error_code ec1 = {}
                  , system::error_code ec2 = {})
   {
      BOOST_ASIO_CORO_REENTER (coro_)
      {
         BOOST_ASIO_CORO_YIELD
         asio::experimental::make_parallel_group(
            [this](auto token)
            {
               return ctor_->runner_.async_run(*conn_, ctor_->cfg_.resolve_timeout, ctor_->cfg_.connect_timeout, token);
            },
            [this](auto token)
            {
               return ctor_->health_checker_.async_check_health(*conn_, token);
            }
         ).async_wait(
            asio::experimental::wait_for_one(),
            std::move(self));

         if (is_cancelled(self)) {
            self.complete(asio::error::operation_aborted);
            return;
         }

         switch (order[0]) {
            case 0: self.complete(ec1); break;
            case 1: self.complete(ec2); break;
            default: BOOST_ASSERT(false);
         }
      }
   }
};

template <class Connector, class Connection>
struct connect_op {
   Connector* ctor_ = nullptr;
   Connection* conn_ = nullptr;
   asio::coroutine coro_{};

   template <class Self>
   void operator()(Self& self, system::error_code ec = {})
   {
      BOOST_ASIO_CORO_REENTER (coro_) for (;;)
      {
         BOOST_ASIO_CORO_YIELD
         ctor_->async_run_check_exec(*conn_, std::move(self));
         ctor_->logger_.on_connection_lost();
         if (is_cancelled(self)) {
            self.complete(asio::error::operation_aborted);
            return;
         }

         conn_->reset_stream();

         if (!conn_->reconnect()) {
            self.complete({});
            return;
         }

         // Wait some time before trying to reconnect.
         ctor_->reconnect_wait_timer_.expires_after(ctor_->cfg_.reconnect_wait_interval);
         BOOST_ASIO_CORO_YIELD
         ctor_->reconnect_wait_timer_.async_wait(std::move(self));
         BOOST_REDIS_CHECK_OP0(;)
      }
   }
};

template <class Executor, class Logger>
class connector {
public:
   connector(Executor ex, connect_config cfg, Logger l)
   : runner_{ex, cfg.addr, l}
   , health_checker_{ex, cfg.health_check_id, cfg.health_check_timeout}
   , reconnect_wait_timer_{ex}
   , cfg_{cfg}
   , logger_{l}
   { }

   template <
      class Connection,
      class CompletionToken = asio::default_completion_token_t<Executor>
   >
   auto async_connect(Connection& conn, CompletionToken token = CompletionToken{})
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(connect_op<connector, Connection>{this, &conn}, token, conn);
   }

   void cancel()
   {
      runner_.cancel();
      health_checker_.cancel();
      reconnect_wait_timer_.cancel();
   }

private:
   using runner_type = redis::detail::runner<Executor, Logger>;
   using health_checker_type = redis::detail::health_checker<Executor>;
   using timer_type = typename runner_type::timer_type;

   template <class, class> friend struct connect_op;
   template <class, class> friend struct run_check_exec_op;
   template <class, class> friend struct run_check_op;
   template <class, class> friend struct hello_op;

   template <class Connection, class CompletionToken>
   auto async_run_check(Connection& conn, CompletionToken token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(run_check_op<connector, Connection>{this, &conn}, token, conn);
   }

   template <class Connection, class CompletionToken>
   auto async_run_check_exec(Connection& conn, CompletionToken token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(run_check_exec_op<connector, Connection>{this, &conn}, token, conn);
   }

   template <class Connection, class CompletionToken>
   auto async_hello(Connection& conn, CompletionToken token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(hello_op<connector, Connection>{this, &conn}, token, conn);
   }

   void add_hello()
   {
      if (!cfg_.username.empty() && !cfg_.password.empty() && !cfg_.clientname.empty())
         req_hello_.push("HELLO", "3", "AUTH", cfg_.username, cfg_.password, "SETNAME", cfg_.clientname);
      else if (cfg_.username.empty() && cfg_.password.empty() && cfg_.clientname.empty())
         req_hello_.push("HELLO", "3");
      else if (cfg_.clientname.empty())
         req_hello_.push("HELLO", "3", "AUTH", cfg_.username, cfg_.password);
      else
         req_hello_.push("HELLO", "3", "SETNAME", cfg_.clientname);

      // Subscribe to channels in the same request that sends HELLO
      // because it has priority over all other requests.
      // TODO: Subscribe to actual channels.
      req_hello_.push("SUBSCRIBE", "channel");
   }

   runner_type runner_;
   health_checker_type health_checker_;
   timer_type reconnect_wait_timer_;
   request req_hello_;
   generic_response resp_hello_;
   connect_config cfg_;
   Logger logger_;
};

} // detail

template <
   class Socket,
   class Logger = logger,
   class CompletionToken = asio::default_completion_token_t<typename Socket::executor_type>
>
auto
async_connect(
   basic_connection<Socket>& conn,
   connect_config cfg = connect_config{},
   Logger l = logger{},
   CompletionToken token = CompletionToken{})
{
   using executor_type = typename Socket::executor_type;
   using connector_type = detail::connector<executor_type, Logger>;
   auto ctor = std::make_shared<connector_type>(conn.get_executor(), cfg, l);
   return ctor->async_connect(conn, asio::consign(std::move(token), ctor));
}

} // boost::redis::experimental

#endif // BOOST_REDIS_CONNECTOR_HPP
