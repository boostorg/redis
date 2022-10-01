/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_CONNECTION_BASE_HPP
#define AEDIS_CONNECTION_BASE_HPP

#include <vector>
#include <queue>
#include <limits>
#include <chrono>
#include <memory>
#include <type_traits>

#include <boost/assert.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/experimental/channel.hpp>

#include <aedis/adapt.hpp>
#include <aedis/endpoint.hpp>
#include <aedis/resp3/request.hpp>
#include <aedis/detail/connection_ops.hpp>

namespace aedis {

/** @brief Base class for high level Redis asynchronous connections.
 *  @ingroup any
 *
 *  This class is not meant to be instantiated directly but as base
 *  class in the CRTP.
 *
 *  @tparam Executor The executor type.
 *  @tparam Derived The derived class type.
 *
 */
template <class Executor, class Derived>
class connection_base {
public:
   /// Executor type.
   using executor_type = Executor;

   /** @brief List of async operations exposed by this class.
    *  
    *  The operations listed below can be cancelled with the `cancel`
    *  member function.
    */
   enum class operation {
      /// Refers to `connection::async_exec` operations.
      exec,
      /// Refers to `connection::async_run` operations.
      run,
      /// Refers to `connection::async_receive_push` operations.
      receive_push,
   };

   /** @brief Constructor
    *
    *  @param ex The executor.
    */
   explicit connection_base(executor_type ex)
   : resv_{ex}
   , ping_timer_{ex}
   , check_idle_timer_{ex}
   , writer_timer_{ex}
   , read_timer_{ex}
   , push_channel_{ex}
   , last_data_{std::chrono::time_point<std::chrono::steady_clock>::min()}
   , req_{{true}}
   {
      writer_timer_.expires_at(std::chrono::steady_clock::time_point::max());
      read_timer_.expires_at(std::chrono::steady_clock::time_point::max());
   }

   /// Returns the executor.
   auto get_executor() {return resv_.get_executor();}

   /** @brief Cancel operations.
    *
    *  @li `operation::exec`: Cancels operations started with
    *  `async_exec`. Has precedence over
    *  `request::config::close_on_connection_lost`
    *  @li operation::run: Cancels the `async_run` operation. Notice
    *  that the preferred way to close a connection is to send a
    *  [QUIT](https://redis.io/commands/quit/) command to the server.
    *  An unresponsive Redis server will also cause the idle-checks to
    *  timeout and lead to `connection::async_run` completing with
    *  `error::idle_timeout`.  Calling `cancel(operation::run)`
    *  directly should be seen as the last option.
    *  @li operation::receive_push: Cancels any ongoing callto
    *  `async_receive_push`.
    *
    *  @param op: The operation to be cancelled.
    *  @returns The number of operations that have been canceled.
    */
   auto cancel(operation op) -> std::size_t
   {
      switch (op) {
         case operation::exec:
         {
            for (auto& e: reqs_) {
               e->stop = true;
               e->timer.cancel_one();
            }

            auto const ret = reqs_.size();
            reqs_ = {};
            return ret;
         }
         case operation::run:
         {
            resv_.cancel();
            derived().close();

            read_timer_.cancel();
            check_idle_timer_.cancel();
            writer_timer_.cancel();
            ping_timer_.cancel();

            auto point = std::stable_partition(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
               return !ptr->req->get_config().close_on_connection_lost;
            });

            // Cancel own pings if there are any waiting.
            std::for_each(point, std::end(reqs_), [](auto const& ptr) {
               ptr->stop = true;
               ptr->timer.cancel();
            });

            reqs_.erase(point, std::end(reqs_));
            return 1U;
         }
         case operation::receive_push:
         {
            push_channel_.cancel();
            return 1U;
         }
         default: BOOST_ASSERT(false); return 0;
      }
   }

   /** @brief Executes a command on the Redis server asynchronously.
    *
    *  This function will send a request to the Redis server and
    *  complete when the response arrives. If the request contains
    *  only commands that don't expect a response, the completion
    *  occurs after it has been written to the underlying stream.
    *  Multiple concurrent calls to this function will be
    *  automatically queued by the implementation.
    *
    *  @param req Request object.
    *  @param adapter Response adapter.
    *  @param token Asio completion token.
    *
    *  For an example see echo_server.cpp. The completion token must
    *  have the following signature
    *
    *  @code
    *  void f(boost::system::error_code, std::size_t);
    *  @endcode
    *
    *  Where the second parameter is the size of the response in
    *  bytes.
    */
   template <
      class Adapter = detail::response_traits<void>::adapter_type,
      class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto async_exec(
      resp3::request const& req,
      Adapter adapter = adapt(),
      CompletionToken token = CompletionToken{})
   {
      BOOST_ASSERT_MSG(req.size() <= adapter.get_supported_response_size(), "Request and adapter have incompatible sizes.");

      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t)
         >(detail::exec_op<Derived, Adapter>{&derived(), &req, adapter}, token, resv_);
   }

   /** @brief Receives server side pushes asynchronously.
    *
    *  Users that expect server pushes should call this function in a
    *  loop. If a push arrives and there is no reader, the connection
    *  will hang and eventually timeout.
    *
    *  @param adapter The response adapter.
    *  @param token The Asio completion token.
    *
    *  For an example see subscriber.cpp. The completion token must
    *  have the following signature
    *
    *  @code
    *  void f(boost::system::error_code, std::size_t);
    *  @endcode
    *
    *  Where the second parameter is the size of the push in
    *  bytes.
    */
   template <
      class Adapter = detail::response_traits<void>::adapter_type,
      class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto async_receive_push(
      Adapter adapter = adapt(),
      CompletionToken token = CompletionToken{})
   {
      auto f = detail::make_adapter_wrapper(adapter);
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t)
         >(detail::receive_push_op<Derived, decltype(f)>{&derived(), f}, token, resv_);
   }

protected:
   template <class Timeouts, class CompletionToken>
   auto
   async_run(endpoint ep, Timeouts ts, CompletionToken token)
   {
      ep_ = std::move(ep);
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::run_op<Derived, Timeouts>{&derived(), ts}, token, resv_);
   }

   template <class Adapter, class Timeouts, class CompletionToken>
   auto async_run(
      endpoint ep,
      resp3::request const& req,
      Adapter adapter,
      Timeouts ts,
      CompletionToken token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t)
         >(detail::runexec_op<Derived, Adapter, Timeouts>
            {&derived(), ep, &req, adapter, ts}, token, resv_);
   }

private:
   using clock_type = std::chrono::steady_clock;
   using clock_traits_type = boost::asio::wait_traits<clock_type>;
   using timer_type = boost::asio::basic_waitable_timer<clock_type, clock_traits_type, executor_type>;
   using resolver_type = boost::asio::ip::basic_resolver<boost::asio::ip::tcp, executor_type>;
   using push_channel_type = boost::asio::experimental::channel<executor_type, void(boost::system::error_code, std::size_t)>;
   using time_point_type = std::chrono::time_point<std::chrono::steady_clock>;

   auto derived() -> Derived& { return static_cast<Derived&>(*this); }

   struct req_info {
      explicit req_info(executor_type ex) : timer{ex} {}
      timer_type timer;
      resp3::request const* req = nullptr;
      std::size_t cmds = 0;
      bool stop = false;
      bool written = false;
   };

   using reqs_type = std::deque<std::shared_ptr<req_info>>;

   template <class, class> friend struct detail::receive_push_op;
   template <class> friend struct detail::reader_op;
   template <class> friend struct detail::writer_op;
   template <class> friend struct detail::ping_op;
   template <class, class> friend struct detail::run_op;
   template <class, class> friend struct detail::exec_op;
   template <class, class> friend struct detail::exec_read_op;
   template <class, class, class> friend struct detail::runexec_op;
   template <class> friend struct detail::resolve_with_timeout_op;
   template <class> friend struct detail::check_idle_op;
   template <class, class> friend struct detail::start_op;
   template <class> friend struct detail::send_receive_op;

   void cancel_push_requests()
   {
      auto point = std::stable_partition(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
         return !(ptr->written && ptr->req->size() == 0);
      });

      std::for_each(point, std::end(reqs_), [](auto const& ptr) {
         ptr->timer.cancel();
      });

      reqs_.erase(point, std::end(reqs_));
   }

   void add_request_info(std::shared_ptr<req_info> const& info)
   {
      reqs_.push_back(info);
      if (derived().is_open() && cmds_ == 0 && write_buffer_.empty())
         writer_timer_.cancel();
   }

   auto make_dynamic_buffer(std::size_t max_read_size = 512)
      { return boost::asio::dynamic_buffer(read_buffer_, max_read_size); }

   template <class CompletionToken>
   auto
   async_resolve_with_timeout(
      std::chrono::steady_clock::duration d,
      CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::resolve_with_timeout_op<Derived>{&derived(), d},
            token, resv_);
   }

   template <class CompletionToken>
   auto reader(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::reader_op<Derived>{&derived()}, token, resv_.get_executor());
   }

   template <class CompletionToken>
   auto writer(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::writer_op<Derived>{&derived()}, token, resv_.get_executor());
   }

   template <
      class Timeouts,
      class CompletionToken>
   auto async_start(Timeouts ts, CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::start_op<Derived, Timeouts>{&derived(), ts}, token, resv_);
   }

   template <class CompletionToken>
   auto
   async_ping(
      std::chrono::steady_clock::duration d,
      CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::ping_op<Derived>{&derived(), d}, token, resv_);
   }

   template <class CompletionToken>
   auto
   async_check_idle(
      std::chrono::steady_clock::duration d,
      CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::check_idle_op<Derived>{&derived(), d}, token, check_idle_timer_);
   }

   template <class Adapter, class CompletionToken>
   auto async_exec_read(Adapter adapter, std::size_t cmds, CompletionToken token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t)
         >(detail::exec_read_op<Derived, Adapter>{&derived(), adapter, cmds}, token, resv_);
   }

   void stage_request(req_info& ri)
   {
      write_buffer_ += ri.req->payload();
      cmds_ += ri.req->size();
      ri.written = true;
   }

   void coalesce_requests()
   {
      BOOST_ASSERT(write_buffer_.empty());
      BOOST_ASSERT(!reqs_.empty());

      stage_request(*reqs_.at(0));

      for (std::size_t i = 1; i < std::size(reqs_); ++i) {
         if (!reqs_.at(i - 1)->req->get_config().coalesce ||
             !reqs_.at(i - 0)->req->get_config().coalesce) {
            break;
         }
         stage_request(*reqs_.at(i));
      }
   }

   void prepare_hello(endpoint const& ep)
   {
      req_.clear();
      if (requires_auth(ep)) {
         req_.push("HELLO", "3", "AUTH", ep.username, ep.password);
      } else {
         req_.push("HELLO", "3");
      }
   }

   auto expect_role(std::string const& expected) -> bool
   {
      if (std::empty(expected))
         return true;

      resp3::node<std::string> role_node;
      role_node.data_type = resp3::type::blob_string;
      role_node.aggregate_size = 1;
      role_node.depth = 1;
      role_node.value = "role";

      auto iter = std::find(std::cbegin(response_), std::cend(response_), role_node);
      if (iter == std::end(response_))
         return false;

      ++iter;
      BOOST_ASSERT(iter != std::cend(response_));
      return iter->value == expected;
   }

   // IO objects
   resolver_type resv_;
protected:
   timer_type ping_timer_;
   endpoint ep_;
   // The result of async_resolve.
   boost::asio::ip::tcp::resolver::results_type endpoints_;

private:
   timer_type check_idle_timer_;
   timer_type writer_timer_;
   timer_type read_timer_;
   push_channel_type push_channel_;

   std::string read_buffer_;
   std::string write_buffer_;
   std::size_t cmds_ = 0;
   reqs_type reqs_;

   // Last time we received data.
   time_point_type last_data_;

   resp3::request req_;
   std::vector<resp3::node<std::string>> response_;
};

} // aedis

#endif // AEDIS_CONNECTION_BASE_HPP
