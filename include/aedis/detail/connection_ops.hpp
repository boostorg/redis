/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_CONNECTION_OPS_HPP
#define AEDIS_CONNECTION_OPS_HPP

#include <array>
#include <algorithm>

#include <boost/assert.hpp>
#include <boost/system.hpp>
#include <boost/asio/write.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <aedis/adapt.hpp>
#include <aedis/error.hpp>
#include <aedis/detail/net.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/detail/exec.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/read.hpp>
#include <aedis/resp3/write.hpp>
#include <aedis/resp3/request.hpp>

#define HANDLER_LOCATION \
  BOOST_ASIO_HANDLER_LOCATION((__FILE__, __LINE__, __func__))

namespace aedis {
namespace detail {

#include <boost/asio/yield.hpp>

template <class Conn>
struct connect_with_timeout_op {
   Conn* conn;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , boost::asio::ip::tcp::endpoint const& = {})
   {
      reenter (coro)
      {
         BOOST_ASSERT(conn->socket_ != nullptr);
         conn->ping_timer_.expires_after(conn->cfg_.connect_timeout);
         yield
         aedis::detail::async_connect(*conn->socket_, conn->ping_timer_, conn->endpoints_, std::move(self));
         self.complete(ec);
      }
   }
};

template <class Conn>
struct resolve_with_timeout_op {
   Conn* conn;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , boost::asio::ip::tcp::resolver::results_type res = {})
   {
      reenter (coro)
      {
         conn->ping_timer_.expires_after(conn->cfg_.resolve_timeout);
         yield
         aedis::detail::async_resolve(
            conn->resv_, conn->ping_timer_,
            conn->cfg_.host, conn->cfg_.port, std::move(self));
         conn->endpoints_ = res;
         self.complete(ec);
      }
   }
};

template <class Conn, class Adapter>
struct receive_push_op {
   Conn* conn = nullptr;
   Adapter adapter;
   std::size_t read_size = 0;
   boost::asio::coroutine coro{};

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t n = 0)
   {
      reenter (coro)
      {
         yield
         conn->push_channel_.async_receive(std::move(self));
         if (ec) {
            self.complete(ec, 0);
            return;
         }

         BOOST_ASSERT(conn->socket_ != nullptr);
         yield
         resp3::async_read(*conn->socket_, conn->make_dynamic_buffer(), adapter, std::move(self));
         if (ec) {
            conn->cancel(Conn::operation::run);
            self.complete(ec, 0);
            return;
         }

         read_size = n;

         yield
         conn->push_channel_.async_send({}, 0, std::move(self));
         self.complete(ec, read_size);
         return;
      }
   }
};

template <class Conn, class Adapter>
struct exec_read_op {
   Conn* conn;
   Adapter adapter;
   std::size_t cmds = 0;
   std::size_t read_size = 0;
   std::size_t index = 0;
   boost::asio::coroutine coro{};

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t n = 0)
   {
      reenter (coro)
      {
         // Loop reading the responses to this request.
         BOOST_ASSERT(!conn->reqs_.empty());
         while (cmds != 0) {
            BOOST_ASSERT(conn->cmds_ != 0);

            //-----------------------------------
            // If we detect a push in the middle of a request we have
            // to hand it to the push consumer. To do that we need
            // some data in the read bufer.
            if (conn->read_buffer_.empty()) {
               BOOST_ASSERT(conn->socket_ != nullptr);
               yield
               boost::asio::async_read_until(*conn->socket_, conn->make_dynamic_buffer(), "\r\n", std::move(self));
               if (ec) {
                  conn->cancel(Conn::operation::run);
                  self.complete(ec, 0);
                  return;
               }
            }

            // If the next request is a push we have to handle it to
            // the receive_push_op wait for it to be done and continue.
            if (resp3::to_type(conn->read_buffer_.front()) == resp3::type::push) {
               yield
               async_send_receive(conn->push_channel_, std::move(self));
               if (ec) {
                  // Notice we don't call cancel_run() as that is the
                  // responsability of the receive_push_op.
                  self.complete(ec, 0);
                  return;
               }

               continue;
            }
            //-----------------------------------

            yield
            resp3::async_read(*conn->socket_, conn->make_dynamic_buffer(),
               [i = index, adpt = adapter] (resp3::node<boost::string_view> const& nd, boost::system::error_code& ec) mutable { adpt(i, nd, ec); },
               std::move(self));

            ++index;

            if (ec) {
               conn->cancel(Conn::operation::run);
               self.complete(ec, 0);
               return;
            }

            read_size += n;

            BOOST_ASSERT(cmds != 0);
            --cmds;

            BOOST_ASSERT(conn->cmds_ != 0);
            --conn->cmds_;
         }

         self.complete({}, read_size);
      }
   }
};

template <class Conn, class Adapter>
struct exec_op {
   using req_info_type = typename Conn::req_info;

   Conn* conn = nullptr;
   resp3::request const* req = nullptr;
   Adapter adapter{};
   std::shared_ptr<req_info_type> info = nullptr;
   std::size_t read_size = 0;
   boost::asio::coroutine coro{};

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t n = 0)
   {
      reenter (coro)
      {
         info = std::allocate_shared<req_info_type>(boost::asio::get_associated_allocator(self), conn->resv_.get_executor());
         info->timer.expires_at(std::chrono::steady_clock::time_point::max());
         info->req = req;
         info->cmds = req->size();
         info->stop = false;

         conn->add_request_info(info);
         yield
         info->timer.async_wait(std::move(self));
         BOOST_ASSERT(conn->socket_ != nullptr);
         BOOST_ASSERT(!!ec);
         if (info->stop) {
            self.complete(ec, 0);
            return;
         }

         BOOST_ASSERT(conn->socket_->is_open());
          
         if (req->size() == 0) {
            self.complete({}, 0);
            return;
         }

         BOOST_ASSERT(!conn->reqs_.empty());
         BOOST_ASSERT(conn->reqs_.front() != nullptr);
         BOOST_ASSERT(conn->cmds_ != 0);
         yield
         conn->async_exec_read(adapter, conn->reqs_.front()->cmds, std::move(self));
         if (ec) {
            self.complete(ec, 0);
            return;
         }

         read_size = n;

         BOOST_ASSERT(!conn->reqs_.empty());
         conn->reqs_.pop_front();

         if (conn->cmds_ == 0) {
            conn->read_timer_.cancel_one();
            if (!conn->reqs_.empty())
               conn->writer_timer_.cancel_one();
         } else {
            BOOST_ASSERT(!conn->reqs_.empty());
            conn->reqs_.front()->timer.cancel_one();
         }

         self.complete({}, read_size);
      }
   }
};

template <class Conn>
struct ping_op {
   Conn* conn;
   boost::asio::coroutine coro{};

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t = 0)
   {
      reenter (coro) for (;;)
      {
         conn->ping_timer_.expires_after(conn->cfg_.ping_interval);
         yield
         conn->ping_timer_.async_wait(std::move(self));
         BOOST_ASSERT(conn->socket_ != nullptr);
         if (ec || !conn->socket_->is_open()) {
            self.complete(ec);
            return;
         }

         conn->req_.clear();
         conn->req_.push("PING");
         conn->req_.close_on_run_completion = true;
         yield
         conn->async_exec(conn->req_, adapt(), std::move(self));
         if (ec) {
            // Notice we don't report error but let the idle check
            // timeout. It is enough to finish the op.
            self.complete({});
            return;
         }
      }
   }
};

template <class Conn>
struct check_idle_op {
   Conn* conn;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro) for (;;)
      {
         conn->check_idle_timer_.expires_after(2 * conn->cfg_.ping_interval);
         yield
         conn->check_idle_timer_.async_wait(std::move(self));
         BOOST_ASSERT(conn->socket_ != nullptr);
         if (ec || !conn->socket_->is_open()) {
            // Notice this is not an error, it was requested from an
            // external op.
            self.complete({});
            return;
         }

         auto const now = std::chrono::steady_clock::now();
         if (conn->last_data_ +  (2 * conn->cfg_.ping_interval) < now) {
            conn->cancel(Conn::operation::run);
            self.complete(error::idle_timeout);
            return;
         }

         conn->last_data_ = now;
      }
   }
};

template <class Conn>
struct start_op {
   Conn* conn;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 4> order = {}
                  , boost::system::error_code ec0 = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {}
                  , boost::system::error_code ec3 = {})
   {
      reenter (coro)
      {
         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return conn->reader(token);},
            [this](auto token) { return conn->writer(token);},
            [this](auto token) { return conn->async_check_idle(token);},
            [this](auto token) { return conn->async_ping(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one_error(),
            std::move(self));

         switch (order[0]) {
           case 0: self.complete(ec0); break;
           case 1: self.complete(ec1); break;
           case 2: self.complete(ec2); break;
           case 3: self.complete(ec3); break;
           default: BOOST_ASSERT(false);
         }
      }
   }
};

template <class Conn>
struct run_one_op {
   Conn* conn;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()(
      Self& self,
      boost::system::error_code ec = {},
      std::size_t = 0)
   {
      reenter (coro)
      {
         yield
         conn->async_resolve_with_timeout(std::move(self));
         if (ec) {
            conn->cancel(Conn::operation::run);
            self.complete(ec);
            return;
         }

         if (conn->cfg_.enable_events) {
            yield
            conn->event_channel_.async_send({}, Conn::event::resolve, std::move(self));
            if (ec) {
               self.complete(ec);
               return;
            }
         }

         conn->socket_ = std::make_shared<typename Conn::next_layer_type>(conn->resv_.get_executor());

         yield
         conn->async_connect_with_timeout(std::move(self));
         if (ec) {
            conn->cancel(Conn::operation::run);
            self.complete(ec);
            return;
         }

         if (conn->cfg_.enable_events) {
            yield
            conn->event_channel_.async_send({}, Conn::event::connect, std::move(self));
            if (ec) {
               self.complete(ec);
               return;
            }
         }

         conn->req_.clear();
         if (!std::empty(conn->cfg_.username) && !std::empty(conn->cfg_.password))
            conn->req_.push("AUTH", conn->cfg_.username, conn->cfg_.password);
         conn->req_.push("HELLO", "3");

         conn->ping_timer_.expires_after(conn->cfg_.ping_interval);

         yield
         async_exec(
            *conn->socket_,
            conn->ping_timer_,
            conn->req_,
            adapter::adapt2(),
            conn->make_dynamic_buffer(),
            std::move(self)
         );

         if (ec) {
            conn->cancel(Conn::operation::run);
            self.complete(ec);
            return;
         }

         if (conn->cfg_.enable_events) {
            yield
            conn->event_channel_.async_send({}, Conn::event::hello, std::move(self));
            if (ec) {
               self.complete(ec);
               return;
            }
         }

         conn->write_buffer_.clear();
         conn->cmds_ = 0;
         std::for_each(std::begin(conn->reqs_), std::end(conn->reqs_), [](auto const& ptr) {
            return ptr->written = false;
         });

         yield
         conn->async_start(std::move(self));
         self.complete(ec);
      }
   }
};

template <class Conn>
struct run_op {
   Conn* conn;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()(
      Self& self,
      boost::system::error_code ec = {},
      std::size_t = 0)
   {
      reenter (coro) for(;;)
      {
         yield
         conn->async_run_one(std::move(self));

         if (!conn->cfg_.enable_reconnect) {
            self.complete(ec);
            return;
         }

         // Consider communicating the return of async_run_one as an
         // event here.

         conn->ping_timer_.expires_after(conn->cfg_.reconnect_interval);
         yield
         conn->ping_timer_.async_wait(std::move(self)); 
      }
   }
};


template <class Conn>
struct writer_op {
   Conn* conn;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      boost::ignore_unused(n);

      reenter (coro) for (;;)
      {
         while (!conn->reqs_.empty() && conn->cmds_ == 0 && conn->write_buffer_.empty()) {
            conn->coalesce_requests();
            yield
            boost::asio::async_write(*conn->socket_, boost::asio::buffer(conn->write_buffer_), std::move(self));
            if (ec) {
               self.complete(ec);
               return;
            }

            // We have to clear the payload right after the read op in
            // order to to use it as a flag that informs there is no
            // ongoing write.
            conn->write_buffer_.clear();
            conn->cancel_push_requests();
         }

         if (conn->socket_->is_open()) {
            yield
            conn->writer_timer_.async_wait(std::move(self));
            // The timer may be canceled either to stop the write op
            // or to proceed to the next write, the difference between
            // the two is that for the former the socket will be
            // closed first. We check for that below.
         }

         if (!conn->socket_->is_open()) {
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
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      boost::ignore_unused(n);

      reenter (coro) for (;;)
      {
         BOOST_ASSERT(conn->socket_->is_open());
         yield
         boost::asio::async_read_until(*conn->socket_, conn->make_dynamic_buffer(), "\r\n", std::move(self));
         if (ec) {
            conn->cancel(Conn::operation::run);
            self.complete(ec);
            return;
         }

         conn->last_data_ = std::chrono::steady_clock::now();

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
         BOOST_ASSERT(!conn->read_buffer_.empty());
         if (resp3::to_type(conn->read_buffer_.front()) == resp3::type::push
             || conn->reqs_.empty()
             || (!conn->reqs_.empty() && conn->reqs_.front()->cmds == 0)) {
            yield
            async_send_receive(conn->push_channel_, std::move(self));
            if (ec) {
               self.complete(ec);
               return;
            }
         } else {
            BOOST_ASSERT(conn->cmds_ != 0);
            BOOST_ASSERT(!conn->reqs_.empty());
            BOOST_ASSERT(conn->reqs_.front()->cmds != 0);
            conn->reqs_.front()->timer.cancel_one();
            yield
            conn->read_timer_.async_wait(std::move(self));
            if (!conn->socket_->is_open()) {
               self.complete({});
               return;
            }
         }

      }
   }
};

template <class Conn, class Adapter>
struct runexec_op {
   Conn* conn;
   resp3::request const* req = nullptr;
   Adapter adapter;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {}
                  , std::size_t n = 0)
   {
      reenter (coro)
      {
         req->close_on_run_completion = true;

         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return conn->async_run(token);},
            [this](auto token) { return conn->async_exec(*req, adapter, token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one_error(),
            std::move(self));

         switch (order[0]) {
           case 0: self.complete(ec1, n); break;
           case 1: self.complete(ec2, n); break;
           default: BOOST_ASSERT(false);
         }
      }
   }
};

#include <boost/asio/unyield.hpp>

} // detail
} // aedis

#endif // AEDIS_CONNECTION_OPS_HPP
