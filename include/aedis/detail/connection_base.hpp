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
#include <memory_resource>

#include <boost/assert.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/experimental/channel.hpp>

#include <aedis/adapt.hpp>
#include <aedis/operation.hpp>
#include <aedis/resp3/request.hpp>
#include <aedis/detail/connection_ops.hpp>

namespace aedis::detail {

/** Base class for high level Redis asynchronous connections.
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
   using executor_type = Executor;
   using this_type = connection_base<Executor, Derived>;

   explicit
   connection_base(executor_type ex, std::pmr::memory_resource* resource)
   : writer_timer_{ex}
   , read_timer_{ex}
   , guarded_op_{ex}
   , read_buffer_{resource}
   , write_buffer_{resource}
   , reqs_{resource}
   {
      writer_timer_.expires_at(std::chrono::steady_clock::time_point::max());
      read_timer_.expires_at(std::chrono::steady_clock::time_point::max());
   }

   auto get_executor() {return writer_timer_.get_executor();}

   auto cancel(operation op) -> std::size_t
   {
      switch (op) {
         case operation::exec:
         {
            return cancel_unwritten_requests();
         }
         case operation::run:
         {
            derived().close();
            read_timer_.cancel();
            writer_timer_.cancel();
            cancel_on_conn_lost();

            return 1U;
         }
         case operation::receive:
         {
            guarded_op_.cancel();
            return 1U;
         }
         default: BOOST_ASSERT(false); return 0;
      }
   }

   auto cancel_unwritten_requests() -> std::size_t
   {
      auto f = [](auto const& ptr)
      {
         BOOST_ASSERT(ptr != nullptr);
         return ptr->is_written();
      };

      auto point = std::stable_partition(std::begin(reqs_), std::end(reqs_), f);

      auto const ret = std::distance(point, std::end(reqs_));

      std::for_each(point, std::end(reqs_), [](auto const& ptr) {
         ptr->stop();
      });

      reqs_.erase(point, std::end(reqs_));
      return ret;
   }

   auto cancel_on_conn_lost() -> std::size_t
   {
      // Must return false if the request should be removed.
      auto cond = [](auto const& ptr)
      {
         BOOST_ASSERT(ptr != nullptr);

         if (ptr->is_written()) {
            return !ptr->get_request().get_config().cancel_if_unresponded;
         } else {
            return !ptr->get_request().get_config().cancel_on_connection_lost;
         }
      };

      auto point = std::stable_partition(std::begin(reqs_), std::end(reqs_), cond);

      auto const ret = std::distance(point, std::end(reqs_));

      std::for_each(point, std::end(reqs_), [](auto const& ptr) {
         ptr->stop();
      });

      reqs_.erase(point, std::end(reqs_));
      std::for_each(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
         return ptr->reset_status();
      });

      return ret;
   }

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
         >(detail::exec_op<Derived, Adapter>{&derived(), &req, adapter}, token, writer_timer_);
   }

   template <
      class Adapter = detail::response_traits<void>::adapter_type,
      class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
   auto async_receive(
      Adapter adapter = adapt(),
      CompletionToken token = CompletionToken{})
   {
      auto f = detail::make_adapter_wrapper(adapter);

      return guarded_op_.async_wait(
         resp3::async_read(derived().next_layer(), make_dynamic_buffer(adapter.get_max_read_size(0)), f, boost::asio::deferred),
         std::move(token));
   }

   template <class CompletionToken>
   auto async_run(CompletionToken token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::run_op<Derived>{&derived()}, token, writer_timer_);
   }

private:
   using clock_type = std::chrono::steady_clock;
   using clock_traits_type = boost::asio::wait_traits<clock_type>;
   using timer_type = boost::asio::basic_waitable_timer<clock_type, clock_traits_type, executor_type>;

   auto derived() -> Derived& { return static_cast<Derived&>(*this); }

   void on_write()
   {
      // We have to clear the payload right after writing it to use it
      // as a flag that informs there is no ongoing write.
      write_buffer_.clear();

      // Notice this must come before the for-each below.
      cancel_push_requests();

      std::for_each(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
         if (ptr->is_staged())
            ptr->mark_written();
      });
   }

   struct req_info {
   public:
      enum class action
      {
         stop,
         proceed,
         none,
      };

      explicit req_info(resp3::request const& req, executor_type ex)
      : timer_{ex}
      , action_{action::none}
      , req_{&req}
      , cmds_{std::size(req)}
      , status_{status::none}
      {
         timer_.expires_at(std::chrono::steady_clock::time_point::max());
      }

      auto proceed()
      {
         timer_.cancel();
         action_ = action::proceed;
      }

      void stop()
      {
         timer_.cancel();
         action_ = action::stop;
      }

      [[nodiscard]] auto is_written() const noexcept
         { return status_ == status::written; }

      [[nodiscard]] auto is_staged() const noexcept
         { return status_ == status::staged; }

      void mark_written() noexcept
         { status_ = status::written; }

      void mark_staged() noexcept
         { status_ = status::staged; }

      void reset_status() noexcept
         { status_ = status::none; }

      [[nodiscard]] auto get_number_of_commands() const noexcept
         { return cmds_; }

      [[nodiscard]] auto get_request() const noexcept -> auto const&
         { return *req_; }

      [[nodiscard]] auto stop_requested() const noexcept
         { return action_ == action::stop;}

      template <class CompletionToken>
      auto async_wait(CompletionToken token)
      {
         return timer_.async_wait(std::move(token));
      }

   private:
      enum class status
      { none
      , staged
      , written
      };

      timer_type timer_;
      action action_;
      resp3::request const* req_;
      std::size_t cmds_;
      status status_;
   };

   void remove_request(std::shared_ptr<req_info> const& info)
   {
      reqs_.erase(std::remove(std::begin(reqs_), std::end(reqs_), info));
   }

   using reqs_type = std::pmr::deque<std::shared_ptr<req_info>>;

   template <class> friend struct detail::reader_op;
   template <class> friend struct detail::writer_op;
   template <class> friend struct detail::run_op;
   template <class, class> friend struct detail::exec_op;
   template <class, class> friend struct detail::exec_read_op;
   template <class> friend struct detail::send_receive_op;

   void cancel_push_requests()
   {
      auto point = std::stable_partition(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
         return !(ptr->is_staged() && ptr->get_request().size() == 0);
      });

      std::for_each(point, std::end(reqs_), [](auto const& ptr) {
         ptr->proceed();
      });

      reqs_.erase(point, std::end(reqs_));
   }

   void add_request_info(std::shared_ptr<req_info> const& info)
   {
      reqs_.push_back(info);

      if (info->get_request().has_hello_priority()) {
         auto rend = std::partition_point(std::rbegin(reqs_), std::rend(reqs_), [](auto const& e) {
               return !e->is_written() && !e->is_staged();
         });

         std::rotate(std::rbegin(reqs_), std::rbegin(reqs_) + 1, rend);
      }

      if (derived().is_open() && cmds_ == 0 && write_buffer_.empty())
         writer_timer_.cancel();
   }

   auto make_dynamic_buffer(std::size_t max_read_size = 512)
      { return boost::asio::dynamic_buffer(read_buffer_, max_read_size); }

   template <class CompletionToken>
   auto reader(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::reader_op<Derived>{&derived()}, token, writer_timer_);
   }

   template <class CompletionToken>
   auto writer(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::writer_op<Derived>{&derived()}, token, writer_timer_);
   }

   template <class Adapter, class CompletionToken>
   auto async_exec_read(Adapter adapter, std::size_t cmds, CompletionToken token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t)
         >(detail::exec_read_op<Derived, Adapter>{&derived(), adapter, cmds}, token, writer_timer_);
   }

   void stage_request(req_info& ri)
   {
      write_buffer_ += ri.get_request().payload();
      cmds_ += ri.get_request().size();
      ri.mark_staged();
   }

   void coalesce_requests()
   {
      // Coalesce the requests and marks them staged. After a
      // successful write staged requests will be marked as written.
      BOOST_ASSERT(write_buffer_.empty());
      BOOST_ASSERT(!reqs_.empty());

      stage_request(*reqs_.at(0));

      for (std::size_t i = 1; i < std::size(reqs_); ++i) {
         if (!reqs_.at(i - 1)->get_request().get_config().coalesce ||
             !reqs_.at(i - 0)->get_request().get_config().coalesce) {
            break;
         }
         stage_request(*reqs_.at(i));
      }
   }

   // Notice we use a timer to simulate a condition-variable. It is
   // also more suitable than a channel and the notify operation does
   // not suspend.
   timer_type writer_timer_;
   timer_type read_timer_;
   detail::guarded_operation<executor_type> guarded_op_;

   std::pmr::string read_buffer_;
   std::pmr::string write_buffer_;
   std::size_t cmds_ = 0;
   reqs_type reqs_;
};

} // aedis

#endif // AEDIS_CONNECTION_BASE_HPP
