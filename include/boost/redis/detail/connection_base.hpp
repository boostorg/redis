/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONNECTION_BASE_HPP
#define BOOST_REDIS_CONNECTION_BASE_HPP

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/detail/helper.hpp>
#include <boost/redis/detail/read.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/operation.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/system.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/assert.hpp>
#include <boost/core/ignore_unused.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>

namespace boost::redis::detail {

template <class Conn>
struct wait_receive_op {
   Conn* conn;
   asio::coroutine coro{};

   template <class Self>
   void
   operator()(Self& self , system::error_code ec = {})
   {
      BOOST_ASIO_CORO_REENTER (coro)
      {
         BOOST_ASIO_CORO_YIELD
         conn->channel_.async_send(system::error_code{}, 0, std::move(self));
         BOOST_REDIS_CHECK_OP0(;);

         BOOST_ASIO_CORO_YIELD
         conn->channel_.async_send(system::error_code{}, 0, std::move(self));
         BOOST_REDIS_CHECK_OP0(;);

         self.complete({});
      }
   }
};

template <class Conn, class Adapter>
class read_next_op {
public:
   using req_info_type = typename Conn::req_info;
   using req_info_ptr = typename std::shared_ptr<req_info_type>;

private:
   Conn* conn_;
   req_info_ptr info_;
   Adapter adapter_;
   std::size_t cmds_ = 0;
   std::size_t read_size_ = 0;
   std::size_t index_ = 0;
   asio::coroutine coro_{};

public:
   read_next_op(Conn& conn, Adapter adapter, req_info_ptr info)
   : conn_{&conn}
   , info_{info}
   , adapter_{adapter}
   , cmds_{info->get_number_of_commands()}
   {}

   auto make_adapter() noexcept
   {
      return [i = index_, adpt = adapter_] (resp3::basic_node<std::string_view> const& nd, system::error_code& ec) mutable { adpt(i, nd, ec); };
   }

   template <class Self>
   void
   operator()( Self& self
             , system::error_code ec = {}
             , std::size_t n = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro_)
      {
         // Loop reading the responses to this request.
         while (cmds_ != 0) {
            if (info_->stop_requested()) {
               self.complete(asio::error::operation_aborted, 0);
               return;
            }

            //-----------------------------------
            // If we detect a push in the middle of a request we have
            // to hand it to the push consumer. To do that we need
            // some data in the read bufer.
            if (conn_->read_buffer_.empty()) {

               if (conn_->derived().use_ssl())
                  BOOST_ASIO_CORO_YIELD asio::async_read_until(conn_->next_layer(), conn_->make_dynamic_buffer(), "\r\n", std::move(self));
               else
                  BOOST_ASIO_CORO_YIELD asio::async_read_until(conn_->next_layer().next_layer(), conn_->make_dynamic_buffer(), "\r\n", std::move(self));

               BOOST_REDIS_CHECK_OP1(conn_->cancel(operation::run););
               if (info_->stop_requested()) {
                  self.complete(asio::error::operation_aborted, 0);
                  return;
               }
            }

            // If the next request is a push we have to handle it to
            // the receive_op wait for it to be done and continue.
            if (resp3::to_type(conn_->read_buffer_.front()) == resp3::type::push) {
               BOOST_ASIO_CORO_YIELD
               conn_->async_wait_receive(std::move(self));
               BOOST_REDIS_CHECK_OP1(conn_->cancel(operation::run););
               continue;
            }
            //-----------------------------------

            if (conn_->derived().use_ssl())
               BOOST_ASIO_CORO_YIELD redis::detail::async_read(conn_->next_layer(), conn_->make_dynamic_buffer(), make_adapter(), std::move(self));
            else
               BOOST_ASIO_CORO_YIELD redis::detail::async_read(conn_->next_layer().next_layer(), conn_->make_dynamic_buffer(), make_adapter(), std::move(self));

            ++index_;

            BOOST_REDIS_CHECK_OP1(conn_->cancel(operation::run););

            read_size_ += n;

            BOOST_ASSERT(cmds_ != 0);
            --cmds_;
         }

         self.complete({}, read_size_);
      }
   }
};

template <class Conn, class Adapter>
struct receive_op {
   Conn* conn;
   Adapter adapter;
   std::size_t read_size = 0;
   asio::coroutine coro{};

   template <class Self>
   void
   operator()( Self& self
             , system::error_code ec = {}
             , std::size_t n = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro)
      {
         BOOST_ASIO_CORO_YIELD
         conn->channel_.async_receive(std::move(self));
         BOOST_REDIS_CHECK_OP1(;);

         if (conn->derived().use_ssl())
            BOOST_ASIO_CORO_YIELD redis::detail::async_read(conn->next_layer(), conn->make_dynamic_buffer(), adapter, std::move(self));
         else
            BOOST_ASIO_CORO_YIELD redis::detail::async_read(conn->next_layer().next_layer(), conn->make_dynamic_buffer(), adapter, std::move(self));

         if (ec || is_cancelled(self)) {
            conn->cancel(operation::run);
            conn->cancel(operation::receive);
            self.complete(!!ec ? ec : asio::error::operation_aborted, {});
            return;
         }

         read_size = n;

         BOOST_ASIO_CORO_YIELD
         conn->channel_.async_receive(std::move(self));
         BOOST_REDIS_CHECK_OP1(;);

         self.complete({}, read_size);
         return;
      }
   }
};

template <class Conn, class Adapter>
struct exec_op {
   using req_info_type = typename Conn::req_info;

   Conn* conn = nullptr;
   request const* req = nullptr;
   Adapter adapter{};
   std::shared_ptr<req_info_type> info = nullptr;
   std::size_t read_size = 0;
   asio::coroutine coro{};

   template <class Self>
   void
   operator()( Self& self
             , system::error_code ec = {}
             , std::size_t n = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro)
      {
         // Check whether the user wants to wait for the connection to
         // be stablished.
         if (req->get_config().cancel_if_not_connected && !conn->is_open()) {
            BOOST_ASIO_CORO_YIELD
            asio::post(std::move(self));
            return self.complete(error::not_connected, 0);
         }

         info = std::allocate_shared<req_info_type>(asio::get_associated_allocator(self), *req, conn->get_executor());

         conn->add_request_info(info);
EXEC_OP_WAIT:
         BOOST_ASIO_CORO_YIELD
         info->async_wait(std::move(self));
         BOOST_ASSERT(ec == asio::error::operation_aborted);

         if (info->stop_requested()) {
            // Don't have to call remove_request as it has already
            // been by cancel(exec).
            return self.complete(ec, 0);
         }

         if (is_cancelled(self)) {
            if (info->is_written()) {
               using c_t = asio::cancellation_type;
               auto const c = self.get_cancellation_state().cancelled();
               if ((c & c_t::terminal) != c_t::none) {
                  // Cancellation requires closing the connection
                  // otherwise it stays in inconsistent state.
                  conn->cancel(operation::run);
                  return self.complete(ec, 0);
               } else {
                  // Can't implement other cancelation types, ignoring.
                  self.get_cancellation_state().clear();
                  goto EXEC_OP_WAIT;
               }
            } else {
               // Cancelation can be honored.
               conn->remove_request(info);
               self.complete(ec, 0);
               return;
            }
         }

         BOOST_ASSERT(conn->is_open());
          
         if (req->size() == 0) {
            // Don't have to call remove_request as it has already
            // been removed.
            return self.complete({}, 0);
         }

         BOOST_ASSERT(!conn->reqs_.empty());
         BOOST_ASSERT(conn->reqs_.front() != nullptr);
         BOOST_ASIO_CORO_YIELD
         conn->async_read_next(adapter, std::move(self));
         BOOST_REDIS_CHECK_OP1(;);

         read_size = n;

         if (info->stop_requested()) {
            // Don't have to call remove_request as it has already
            // been by cancel(exec).
            return self.complete(ec, 0);
         }

         BOOST_ASSERT(!conn->reqs_.empty());
         conn->reqs_.pop_front();

         if (conn->is_waiting_response()) {
            BOOST_ASSERT(!conn->reqs_.empty());
            conn->reqs_.front()->proceed();
         } else {
            conn->read_timer_.cancel_one();
         }

         self.complete({}, read_size);
      }
   }
};

template <class Conn, class Logger>
struct run_op {
   Conn* conn = nullptr;
   Logger logger_;
   asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , system::error_code ec0 = {}
                  , system::error_code ec1 = {})
   {
      BOOST_ASIO_CORO_REENTER (coro)
      {
         conn->write_buffer_.clear();
         conn->read_buffer_.clear();

         BOOST_ASIO_CORO_YIELD
         asio::experimental::make_parallel_group(
            [this](auto token) { return conn->reader(token);},
            [this](auto token) { return conn->writer(logger_, token);}
         ).async_wait(
            asio::experimental::wait_for_one(),
            std::move(self));

         if (is_cancelled(self)) {
            self.complete(asio::error::operation_aborted);
            return;
         }

         switch (order[0]) {
           case 0: self.complete(ec0); break;
           case 1: self.complete(ec1); break;
           default: BOOST_ASSERT(false);
         }
      }
   }
};

template <class Conn, class Logger>
struct writer_op {
   Conn* conn_;
   Logger logger_;
   asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , system::error_code ec = {}
                  , std::size_t n = 0)
   {
      ignore_unused(n);

      BOOST_ASIO_CORO_REENTER (coro) for (;;)
      {
         while (conn_->coalesce_requests()) {
            if (conn_->derived().use_ssl())
               BOOST_ASIO_CORO_YIELD asio::async_write(conn_->next_layer(), asio::buffer(conn_->write_buffer_), std::move(self));
            else
               BOOST_ASIO_CORO_YIELD asio::async_write(conn_->next_layer().next_layer(), asio::buffer(conn_->write_buffer_), std::move(self));

            logger_.on_write(ec, conn_->write_buffer_);
            BOOST_REDIS_CHECK_OP0(conn_->cancel(operation::run););

            conn_->on_write();

            // A socket.close() may have been called while a
            // successful write might had already been queued, so we
            // have to check here before proceeding.
            if (!conn_->is_open()) {
               self.complete({});
               return;
            }
         }

         BOOST_ASIO_CORO_YIELD
         conn_->writer_timer_.async_wait(std::move(self));
         if (!conn_->is_open() || is_cancelled(self)) {
            // Notice this is not an error of the op, stoping was
            // requested from the outside, so we complete with
            // success.
            self.complete({});
            return;
         }
      }
   }
};

template <class Conn>
struct reader_op {
   Conn* conn;
   asio::coroutine coro{};

   bool as_push() const
   {
      return
         (resp3::to_type(conn->read_buffer_.front()) == resp3::type::push)
          || conn->reqs_.empty()
          || (!conn->reqs_.empty() && conn->reqs_.front()->get_number_of_commands() == 0)
          || !conn->is_waiting_response(); // Added to deal with MONITOR.
   }

   template <class Self>
   void operator()( Self& self
                  , system::error_code ec = {}
                  , std::size_t n = 0)
   {
      ignore_unused(n);

      BOOST_ASIO_CORO_REENTER (coro) for (;;)
      {
         if (conn->derived().use_ssl())
            BOOST_ASIO_CORO_YIELD asio::async_read_until(conn->next_layer(), conn->make_dynamic_buffer(), "\r\n", std::move(self));
         else
            BOOST_ASIO_CORO_YIELD asio::async_read_until(conn->next_layer().next_layer(), conn->make_dynamic_buffer(), "\r\n", std::move(self));

         if (ec == asio::error::eof) {
            conn->cancel(operation::run);
            return self.complete({}); // EOFINAE: EOF is not an error.
         }

         BOOST_REDIS_CHECK_OP0(conn->cancel(operation::run););

         // We handle unsolicited events in the following way
         //
         // 1. Its resp3 type is a push.
         //
         // 2. A non-push type is received with an empty requests
         //    queue. I have noticed this is possible (e.g. -MISCONF).
         //    I expect them to have type push so we can distinguish
         //    them from responses to commands, but it is a
         //    simple-error. If we are lucky enough to receive them
         //    when the command queue is empty we can treat them as
         //    server pushes, otherwise it is impossible to handle
         //    them properly
         //
         // 3. The request does not expect any response but we got
         //    one. This may happen if for example, subscribe with
         //    wrong syntax.
         //
         // Useful links:
         //
         // - https://github.com/redis/redis/issues/11784
         // - https://github.com/redis/redis/issues/6426
         //
         BOOST_ASSERT(!conn->read_buffer_.empty());
         if (as_push()) {
            BOOST_ASIO_CORO_YIELD
            conn->async_wait_receive(std::move(self));
         } else {
            BOOST_ASSERT_MSG(conn->is_waiting_response(), "Not waiting for a response (using MONITOR command perhaps?)");
            BOOST_ASSERT(!conn->reqs_.empty());
            BOOST_ASSERT(conn->reqs_.front()->get_number_of_commands() != 0);
            conn->reqs_.front()->proceed();
            BOOST_ASIO_CORO_YIELD
            conn->read_timer_.async_wait(std::move(self));
            ec = {};
         }

         if (!conn->is_open() || ec || is_cancelled(self)) {
            conn->cancel(operation::run);
            self.complete(asio::error::basic_errors::operation_aborted);
            return;
         }
      }
   }
};

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

   connection_base(executor_type ex)
   : writer_timer_{ex}
   , read_timer_{ex}
   , channel_{ex}
   {
      writer_timer_.expires_at(std::chrono::steady_clock::time_point::max());
      read_timer_.expires_at(std::chrono::steady_clock::time_point::max());
   }

   auto get_executor() {return writer_timer_.get_executor();}

   auto cancel_impl(operation op) -> std::size_t
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
            return cancel_on_conn_lost();
         }
         case operation::receive:
         {
            channel_.cancel();
            return 1U;
         }
         default: /* ignore */; return 0;
      }
   }

   auto cancel(operation op) -> std::size_t
   {
      if (op == operation::all) {
         std::size_t ret = 0;
         ret += cancel_impl(operation::run);
         ret += cancel_impl(operation::receive);
         ret += cancel_impl(operation::exec);
         return ret;
      } 

      return cancel_impl(op);
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

   template <class Response, class CompletionToken>
   auto async_exec(request const& req, Response& resp, CompletionToken token)
   {
      using namespace boost::redis::adapter;
      auto f = boost_redis_adapt(resp);
      BOOST_ASSERT_MSG(req.size() <= f.get_supported_response_size(), "Request and response have incompatible sizes.");

      return asio::async_compose
         < CompletionToken
         , void(system::error_code, std::size_t)
         >(exec_op<Derived, decltype(f)>{&derived(), &req, f}, token, writer_timer_);
   }

   template <class Response, class CompletionToken>
   auto async_receive(Response& response, CompletionToken token)
   {
      using namespace boost::redis::adapter;
      auto g = boost_redis_adapt(response);
      auto f = adapter::detail::make_adapter_wrapper(g);

      return asio::async_compose
         < CompletionToken
         , void(system::error_code, std::size_t)
         >(receive_op<Derived, decltype(f)>{&derived(), f}, token, channel_);
   }

   template <class Logger, class CompletionToken>
   auto async_run_impl(Logger l, CompletionToken token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(run_op<Derived, Logger>{&derived(), l}, token, writer_timer_);
   }

   void set_max_buffer_read_size(std::size_t max_read_size) noexcept
      {max_read_size_ = max_read_size;}

   // Reserves memory in the read and write buffer.
   void reserve(std::size_t read, std::size_t write)
   {
      read_buffer_.reserve(read);
      write_buffer_.reserve(write);
   }

private:
   using clock_type = std::chrono::steady_clock;
   using clock_traits_type = asio::wait_traits<clock_type>;
   using timer_type = asio::basic_waitable_timer<clock_type, clock_traits_type, executor_type>;
   using channel_type = asio::experimental::channel<executor_type, void(system::error_code, std::size_t)>;

   auto derived() -> Derived& { return static_cast<Derived&>(*this); }

   void on_write()
   {
      // We have to clear the payload right after writing it to use it
      // as a flag that informs there is no ongoing write.
      write_buffer_.clear();

      // Notice this must come before the for-each below.
      cancel_push_requests();

      // There is small optimization possible here: traverse only the
      // partition of unwritten requests instead of them all.
      std::for_each(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
         BOOST_ASSERT_MSG(ptr != nullptr, "Expects non-null pointer.");
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

      explicit req_info(request const& req, executor_type ex)
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

      [[nodiscard]] auto is_waiting_write() const noexcept
         { return !is_written() && !is_staged(); }

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
      request const* req_;
      std::size_t cmds_;
      status status_;
   };

   void remove_request(std::shared_ptr<req_info> const& info)
   {
      reqs_.erase(std::remove(std::begin(reqs_), std::end(reqs_), info));
   }

   using reqs_type = std::deque<std::shared_ptr<req_info>>;

   template <class> friend struct reader_op;
   template <class, class> friend struct writer_op;
   template <class, class> friend struct run_op;
   template <class, class> friend struct exec_op;
   template <class, class> friend class read_next_op;
   template <class, class> friend struct receive_op;
   template <class> friend struct wait_receive_op;

   template <class CompletionToken>
   auto async_wait_receive(CompletionToken token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(wait_receive_op<Derived>{&derived()}, token, channel_);
   }

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

   [[nodiscard]] bool is_writing() const noexcept
   {
      return !write_buffer_.empty();
   }

   void add_request_info(std::shared_ptr<req_info> const& info)
   {
      reqs_.push_back(info);

      if (info->get_request().has_hello_priority()) {
         auto rend = std::partition_point(std::rbegin(reqs_), std::rend(reqs_), [](auto const& e) {
               return e->is_waiting_write();
         });

         std::rotate(std::rbegin(reqs_), std::rbegin(reqs_) + 1, rend);
      }

      if (derived().is_open() && !is_writing())
         writer_timer_.cancel();
   }

   auto make_dynamic_buffer()
      { return asio::dynamic_buffer(read_buffer_, max_read_size_); }

   template <class CompletionToken>
   auto reader(CompletionToken&& token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(reader_op<Derived>{&derived()}, token, writer_timer_);
   }

   template <class CompletionToken, class Logger>
   auto writer(Logger l, CompletionToken&& token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code)
         >(writer_op<Derived, Logger>{&derived(), l}, token, writer_timer_);
   }

   template <class Adapter, class CompletionToken>
   auto async_read_next(Adapter adapter, CompletionToken token)
   {
      return asio::async_compose
         < CompletionToken
         , void(system::error_code, std::size_t)
         >(read_next_op<Derived, Adapter>{derived(), adapter, reqs_.front()}, token, writer_timer_);
   }

   [[nodiscard]] bool coalesce_requests()
   {
      // Coalesces the requests and marks them staged. After a
      // successful write staged requests will be marked as written.
      auto const point = std::partition_point(std::cbegin(reqs_), std::cend(reqs_), [](auto const& ri) {
            return !ri->is_waiting_write();
      });

      std::for_each(point, std::cend(reqs_), [this](auto const& ri) {
         // Stage the request.
         write_buffer_ += ri->get_request().payload();
         ri->mark_staged();
      });

      return point != std::cend(reqs_);
   }

   bool is_waiting_response() const noexcept
   {
      return !std::empty(reqs_) && reqs_.front()->is_written();
   }

   // Notice we use a timer to simulate a condition-variable. It is
   // also more suitable than a channel and the notify operation does
   // not suspend.
   timer_type writer_timer_;
   timer_type read_timer_;
   channel_type channel_;

   std::string read_buffer_;
   std::string write_buffer_;
   reqs_type reqs_;
   std::size_t max_read_size_ = (std::numeric_limits<std::size_t>::max)();
};

} // boost::redis::detail

#endif // BOOST_REDIS_CONNECTION_BASE_HPP
