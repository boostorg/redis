/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RUNNER_HPP
#define BOOST_REDIS_RUNNER_HPP

// Has to included before promise.hpp to build on msvc.
#include <boost/redis/detail/helper.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/address.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <chrono>

namespace boost::redis::detail {

template <class Runner>
struct resolve_op {
   Runner* runner = nullptr;
   asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , system::error_code ec1 = {}
                  , asio::ip::tcp::resolver::results_type res = {}
                  , system::error_code ec2 = {})
   {
      BOOST_ASIO_CORO_REENTER (coro)
      {
         BOOST_ASIO_CORO_YIELD
         asio::experimental::make_parallel_group(
            [this](auto token)
            {
               return runner->resv_.async_resolve(runner->addr_.host, runner->addr_.port, token);
            },
            [this](auto token) { return runner->timer_.async_wait(token);}
         ).async_wait(
            asio::experimental::wait_for_one(),
            std::move(self));

         runner->logger_.on_resolve(ec1, res);

         if (is_cancelled(self)) {
            self.complete(asio::error::operation_aborted);
            return;
         }

         switch (order[0]) {
            case 0: {
               // Resolver completed first.
               runner->endpoints_ = res;
               self.complete(ec1);
            } break;

            case 1: {
               if (ec2) {
                  // Timer completed first with error, perhaps a
                  // cancellation going on.
                  self.complete(ec2);
               } else {
                  // Timer completed first without an error, this is a
                  // resolve timeout.
                  self.complete(error::resolve_timeout);
               }
            } break;

            default: BOOST_ASSERT(false);
         }
      }
   }
};

template <class Runner, class Stream>
struct connect_op {
   Runner* runner = nullptr;
   Stream* stream = nullptr;
   asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> const& order = {}
                  , system::error_code const& ec1 = {}
                  , asio::ip::tcp::endpoint const& ep= {}
                  , system::error_code const& ec2 = {})
   {
      BOOST_ASIO_CORO_REENTER (coro)
      {
         BOOST_ASIO_CORO_YIELD
         asio::experimental::make_parallel_group(
            [this](auto token)
            {
               auto f = [](system::error_code const&, auto const&) { return true; };
               return asio::async_connect(*stream, runner->endpoints_, f, token);
            },
            [this](auto token) { return runner->timer_.async_wait(token);}
         ).async_wait(
            asio::experimental::wait_for_one(),
            std::move(self));

         runner->logger_.on_connect(ec1, ep);

         if (is_cancelled(self)) {
            self.complete(asio::error::operation_aborted);
            return;
         }

         switch (order[0]) {
            case 0: {
               self.complete(ec1);
            } break;
            case 1:
            {
               if (ec2) {
                  self.complete(ec2);
               } else {
                  self.complete(error::connect_timeout);
               }
            } break;

            default: BOOST_ASSERT(false);
         }
      }
   }
};

template <class Runner, class Connection>
struct runner_op {
   Runner* runner = nullptr;
   Connection* conn = nullptr;
   std::chrono::steady_clock::duration resolve_timeout;
   std::chrono::steady_clock::duration connect_timeout;
   asio::coroutine coro{};

   template <class Self>
   void operator()(Self& self, system::error_code ec = {})
   {
      BOOST_ASIO_CORO_REENTER (coro)
      {
         runner->timer_.expires_after(resolve_timeout);
         BOOST_ASIO_CORO_YIELD
         runner->async_resolve(std::move(self));
         BOOST_REDIS_CHECK_OP0(;)

         runner->timer_.expires_after(connect_timeout);
         BOOST_ASIO_CORO_YIELD
         runner->async_connect(conn->next_layer(), std::move(self));
         BOOST_REDIS_CHECK_OP0(;)

         BOOST_ASIO_CORO_YIELD
         conn->async_run(std::move(self));
         BOOST_REDIS_CHECK_OP0(;)
         self.complete({});
      }
   }
};

template <class Executor, class Logger>
class runner {
public:
   using timer_type =
      asio::basic_waitable_timer<
         std::chrono::steady_clock,
         asio::wait_traits<std::chrono::steady_clock>,
         Executor>;

   runner(Executor ex, address addr, Logger l = Logger{})
   : resv_{ex}
   , timer_{ex}
   , addr_{addr}
   , logger_{l}
   {}

   template <class CompletionToken>
   auto async_resolve(CompletionToken&& token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(resolve_op<runner>{this}, token, resv_);
   }

   template <class Stream, class CompletionToken>
   auto async_connect(Stream& stream, CompletionToken&& token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(connect_op<runner, Stream>{this, &stream}, token, resv_);
   }

   template <class Connection, class CompletionToken>
   auto
   async_run(
      Connection& conn,
      std::chrono::steady_clock::duration resolve_timeout,
      std::chrono::steady_clock::duration connect_timeout,
      CompletionToken&& token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(runner_op<runner, Connection>{this, &conn, resolve_timeout, connect_timeout}, token, resv_);
   }

   void cancel()
   {
      resv_.cancel();
      timer_.cancel();
   }

private:
   using resolver_type = asio::ip::basic_resolver<asio::ip::tcp, Executor>;

   template <class, class> friend struct runner_op;
   template <class, class> friend struct connect_op;
   template <class> friend struct resolve_op;

   resolver_type resv_;
   timer_type timer_;
   address addr_;
   asio::ip::tcp::resolver::results_type endpoints_;
   Logger logger_;
};

} // boost::redis::detail

#endif // BOOST_REDIS_RUNNER_HPP
