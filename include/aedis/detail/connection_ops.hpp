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

#include <boost/asio/yield.hpp>

namespace aedis::detail {

template <class Conn, class Timer>
struct connect_with_timeout_op {
   Conn* conn = nullptr;
   boost::asio::ip::tcp::resolver::results_type const* endpoints = nullptr;
   typename Conn::timeouts ts;
   Timer* timer = nullptr;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , boost::asio::ip::tcp::endpoint const& = {})
   {
      reenter (coro)
      {
         timer->expires_after(ts.connect_timeout);
         yield detail::async_connect(conn->next_layer(), *timer, *endpoints, std::move(self));
         AEDIS_CHECK_OP0();
         self.complete({});
      }
   }
};

template <class Conn>
struct resolve_with_timeout_op {
   Conn* conn = nullptr;
   std::chrono::steady_clock::duration resolve_timeout{};
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , boost::asio::ip::tcp::resolver::results_type const& res = {})
   {
      reenter (coro)
      {
         conn->ping_timer_.expires_after(resolve_timeout);
         yield
         aedis::detail::async_resolve(
            conn->resv_, conn->ping_timer_,
            conn->ep_.host, conn->ep_.port, std::move(self));
         AEDIS_CHECK_OP0();
         conn->endpoints_ = res;
         self.complete({});
      }
   }
};

template <class Conn, class Adapter>
struct receive_op {
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
         yield conn->push_channel_.async_receive(std::move(self));
         AEDIS_CHECK_OP1();

         yield
         resp3::async_read(
            conn->next_layer(),
            conn->make_dynamic_buffer(adapter.get_max_read_size(0)),
            adapter, std::move(self));

         // cancel(receive) is needed to cancel the channel, otherwise
         // the read operation will be blocked forever see
         // test_push_adapter.
         AEDIS_CHECK_OP1(conn->cancel(operation::run); conn->cancel(operation::receive));

         read_size = n;

         yield conn->push_channel_.async_send({}, 0, std::move(self));
         AEDIS_CHECK_OP1();

         self.complete({}, read_size);
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
               yield
               boost::asio::async_read_until(
                  conn->next_layer(),
                  conn->make_dynamic_buffer(),
                  "\r\n", std::move(self));
               AEDIS_CHECK_OP1(conn->cancel(operation::run));
            }

            // If the next request is a push we have to handle it to
            // the receive_op wait for it to be done and continue.
            if (resp3::to_type(conn->read_buffer_.front()) == resp3::type::push) {
               yield
               async_send_receive(conn->push_channel_, std::move(self));
               AEDIS_CHECK_OP1(conn->cancel(operation::run));
               continue;
            }
            //-----------------------------------

            yield
            resp3::async_read(
               conn->next_layer(),
               conn->make_dynamic_buffer(adapter.get_max_read_size(index)),
                  [i = index, adpt = adapter] (resp3::node<boost::string_view> const& nd, boost::system::error_code& ec) mutable { adpt(i, nd, ec); },
                  std::move(self));

            ++index;

            AEDIS_CHECK_OP1(conn->cancel(operation::run));

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
         // Check whether the user wants to wait for the connection to
         // be stablished.
         // TODO: is_open below reflects only whether a TCP connection
         // has been stablished. We need a variable that informs
         // whether HELLO was successfull and we are connected with
         // Redis.
         if (req->get_config().cancel_if_not_connected && !conn->is_open())
            return self.complete(error::not_connected, 0);

         info = std::allocate_shared<req_info_type>(boost::asio::get_associated_allocator(self), *req, conn->resv_.get_executor());

         conn->add_request_info(info);
EXEC_OP_WAIT:
         yield info->async_wait(std::move(self));
         BOOST_ASSERT(ec == boost::asio::error::operation_aborted);

         if (info->get_action() == Conn::req_info::action::stop) {
            return self.complete(ec, 0);
         }

         if (is_cancelled(self)) {
            if (info->is_written()) {
               self.get_cancellation_state().clear();
               goto EXEC_OP_WAIT; // Too late, can't cancel.
            } else {
               conn->remove_request(info);
               self.complete(ec, 0);
               return;
            }
         }

         BOOST_ASSERT(conn->is_open());
          
         if (req->size() == 0)
            return self.complete({}, 0);

         BOOST_ASSERT(!conn->reqs_.empty());
         BOOST_ASSERT(conn->reqs_.front() != nullptr);
         BOOST_ASSERT(conn->cmds_ != 0);
         yield
         conn->async_exec_read(adapter, conn->reqs_.front()->get_number_of_commands(), std::move(self));
         AEDIS_CHECK_OP1();

         read_size = n;

         BOOST_ASSERT(!conn->reqs_.empty());
         conn->reqs_.pop_front();

         if (conn->cmds_ == 0) {
            conn->read_timer_.cancel_one();
            if (!conn->reqs_.empty())
               conn->writer_timer_.cancel_one();
         } else {
            BOOST_ASSERT(!conn->reqs_.empty());
            conn->reqs_.front()->proceed();
         }

         self.complete({}, read_size);
      }
   }
};

template <class Conn>
struct ping_op {
   Conn* conn{};
   std::chrono::steady_clock::duration ping_interval{};
   boost::asio::coroutine coro{};

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t = 0)
   {
      reenter (coro) for (;;)
      {
         conn->ping_timer_.expires_after(ping_interval);
         yield conn->ping_timer_.async_wait(std::move(self));
         if (!conn->is_open() || ec || is_cancelled(self)) {
            // Checking for is_open is necessary becuse the timer can
            // complete with success although cancel has been called.
            self.complete({});
            return;
         }

         conn->req_.clear();
         conn->req_.push("PING");
         yield conn->async_exec(conn->req_, adapt(), std::move(self));
         if (!conn->is_open() || is_cancelled(self)) {
            // Checking for is_open is necessary to avoid
            // looping back on the timer although cancel has been
            // called.
            return self.complete({});
         }
      }
   }
};

template <class Conn>
struct check_idle_op {
   Conn* conn{};
   std::chrono::steady_clock::duration ping_interval{};
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro) for (;;)
      {
         conn->check_idle_timer_.expires_after(2 * ping_interval);
         yield conn->check_idle_timer_.async_wait(std::move(self));
         if (!conn->is_open() || ec || is_cancelled(self)) {
            // Checking for is_open is necessary becuse the timer can
            // complete with success although cancel has been called.
            return self.complete({});
         }

         auto const now = std::chrono::steady_clock::now();
         if (conn->last_data_ +  (2 * ping_interval) < now) {
            conn->cancel(operation::run);
            self.complete(error::idle_timeout);
            return;
         }

         conn->last_data_ = now;
      }
   }
};

template <class Conn, class Timeouts>
struct start_op {
   Conn* conn;
   Timeouts ts;
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
            [this](auto token) { return conn->async_check_idle(ts.ping_interval, token);},
            [this](auto token) { return conn->async_ping(ts.ping_interval, token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         if (is_cancelled(self)) {
            self.complete(boost::asio::error::operation_aborted);
            return;
         }

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

inline
auto check_resp3_handshake_failed(std::vector<resp3::node<std::string>> const& resp) -> bool
{
   return std::size(resp) == 1 && 
         (resp.front().data_type == resp3::type::simple_error ||
          resp.front().data_type == resp3::type::blob_error ||
          resp.front().data_type == resp3::type::null);
}

template <class Conn, class Timeouts>
struct run_op {
   Conn* conn = nullptr;
   Timeouts ts;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()(
      Self& self,
      boost::system::error_code ec = {},
      std::size_t = 0)
   {
      reenter (coro)
      {
         yield conn->async_resolve_with_timeout(ts.resolve_timeout, std::move(self));
         AEDIS_CHECK_OP0(conn->cancel(operation::run));

         yield conn->derived().async_connect(conn->endpoints_, ts, conn->ping_timer_, std::move(self));
         AEDIS_CHECK_OP0(conn->cancel(operation::run));

         conn->prepare_hello(conn->ep_);
         conn->ping_timer_.expires_after(ts.resp3_handshake_timeout);
         conn->response_.clear();

         yield
         resp3::detail::async_exec(
            conn->next_layer(),
            conn->ping_timer_,
            conn->req_,
            adapter::adapt2(conn->response_),
            conn->make_dynamic_buffer(),
            std::move(self)
         );

         AEDIS_CHECK_OP0(conn->cancel(operation::run));

         if (check_resp3_handshake_failed(conn->response_)) {
            conn->cancel(operation::run);
            self.complete(error::resp3_handshake_error);
            return;
         }

         conn->ep_.password.clear();

         if (!conn->expect_role(conn->ep_.role)) {
            conn->cancel(operation::run);
            self.complete(error::unexpected_server_role);
            return;
         }

         conn->write_buffer_.clear();
         conn->cmds_ = 0;

         yield conn->async_start(ts, std::move(self));
         AEDIS_CHECK_OP0();
         self.complete({});
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
            boost::asio::async_write(conn->next_layer(), boost::asio::buffer(conn->write_buffer_), std::move(self));
            AEDIS_CHECK_OP0(conn->cancel(operation::run));

            conn->on_write();

            // A socket.close() may have been called while a
            // successful write might had already been queued, so we
            // have to check here before proceeding.
            if (!conn->is_open()) {
               self.complete({});
               return;
            }
         }

         yield conn->writer_timer_.async_wait(std::move(self));
         if (!conn->is_open() || is_cancelled(self)) {
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
         yield
         boost::asio::async_read_until(
            conn->next_layer(),
            conn->make_dynamic_buffer(),
            "\r\n", std::move(self));
         AEDIS_CHECK_OP0(conn->cancel(operation::run));

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
             || (!conn->reqs_.empty() && conn->reqs_.front()->get_number_of_commands() == 0)) {
            yield async_send_receive(conn->push_channel_, std::move(self));
            if (!conn->is_open() || ec || is_cancelled(self)) {
               conn->cancel(operation::run);
               self.complete(boost::asio::error::basic_errors::operation_aborted);
               return;
            }
         } else {
            BOOST_ASSERT(conn->cmds_ != 0);
            BOOST_ASSERT(!conn->reqs_.empty());
            BOOST_ASSERT(conn->reqs_.front()->get_number_of_commands() != 0);
            conn->reqs_.front()->proceed();
            yield conn->read_timer_.async_wait(std::move(self));
            if (!conn->is_open() || is_cancelled(self)) {
               // Added this cancel here to make sure any outstanding
               // ping is cancelled.
               conn->cancel(operation::run);
               self.complete(boost::asio::error::basic_errors::operation_aborted);
               return;
            }
         }
      }
   }
};

} // aedis::detail

#include <boost/asio/unyield.hpp>
#endif // AEDIS_CONNECTION_OPS_HPP
