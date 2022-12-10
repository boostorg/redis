/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_CONNECTION_OPS_HPP
#define AEDIS_CONNECTION_OPS_HPP

#include <array>
#include <algorithm>
#include <string_view>

#include <boost/assert.hpp>
#include <boost/system.hpp>
#include <boost/asio/write.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <aedis/adapt.hpp>
#include <aedis/error.hpp>
#include <aedis/detail/guarded_operation.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/read.hpp>
#include <aedis/resp3/write.hpp>
#include <aedis/resp3/request.hpp>

#include <boost/asio/yield.hpp>

namespace aedis::detail {

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
               AEDIS_CHECK_OP1(conn->cancel(operation::run););
            }

            // If the next request is a push we have to handle it to
            // the receive_op wait for it to be done and continue.
            if (resp3::to_type(conn->read_buffer_.front()) == resp3::type::push) {
               yield conn->guarded_op_.async_run(std::move(self));
               AEDIS_CHECK_OP1(conn->cancel(operation::run););
               continue;
            }
            //-----------------------------------

            yield
            resp3::async_read(
               conn->next_layer(),
               conn->make_dynamic_buffer(adapter.get_max_read_size(index)),
                  [i = index, adpt = adapter] (resp3::node<std::string_view> const& nd, boost::system::error_code& ec) mutable { adpt(i, nd, ec); },
                  std::move(self));

            ++index;

            AEDIS_CHECK_OP1(conn->cancel(operation::run););

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
         if (req->get_config().cancel_if_not_connected && !conn->is_open()) {
            return self.complete(error::not_connected, 0);
         }

         info = std::allocate_shared<req_info_type>(boost::asio::get_associated_allocator(self), *req, conn->get_executor());

         conn->add_request_info(info);
EXEC_OP_WAIT:
         yield info->async_wait(std::move(self));
         BOOST_ASSERT(ec == boost::asio::error::operation_aborted);

         if (info->get_action() == Conn::req_info::action::stop) {
            // Don't have to call remove_request as it has already
            // been by cancel(exec).
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
          
         if (req->size() == 0) {
            // Don't have to call remove_request as it has already
            // been removed.
            return self.complete({}, 0);
         }

         BOOST_ASSERT(!conn->reqs_.empty());
         BOOST_ASSERT(conn->reqs_.front() != nullptr);
         BOOST_ASSERT(conn->cmds_ != 0);
         yield
         conn->async_exec_read(adapter, conn->reqs_.front()->get_number_of_commands(), std::move(self));
         if (is_cancelled(self)) {
            conn->remove_request(info);
            return self.complete(boost::asio::error::operation_aborted, {});
         }

         if (ec) {
            return self.complete(ec, {});
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
            conn->reqs_.front()->proceed();
         }

         self.complete({}, read_size);
      }
   }
};

template <class Conn>
struct run_op {
   Conn* conn = nullptr;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec0 = {}
                  , boost::system::error_code ec1 = {})
   {
      reenter (coro)
      {
         conn->write_buffer_.clear();
         conn->cmds_ = 0;

         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return conn->reader(token);},
            [this](auto token) { return conn->writer(token);}
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
           default: BOOST_ASSERT(false);
         }
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
            AEDIS_CHECK_OP0(conn->cancel(operation::run););

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

         if (ec == boost::asio::error::eof) {
            conn->cancel(operation::run);
            return self.complete({}); // EOFINAE: EOF is not an error.
         }

         AEDIS_CHECK_OP0(conn->cancel(operation::run););

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
            yield conn->guarded_op_.async_run(std::move(self));
         } else {
            BOOST_ASSERT(conn->cmds_ != 0);
            BOOST_ASSERT(!conn->reqs_.empty());
            BOOST_ASSERT(conn->reqs_.front()->get_number_of_commands() != 0);
            conn->reqs_.front()->proceed();
            yield conn->read_timer_.async_wait(std::move(self));
            ec = {};
         }

         if (!conn->is_open() || ec || is_cancelled(self)) {
            conn->cancel(operation::run);
            self.complete(boost::asio::error::basic_errors::operation_aborted);
            return;
         }
      }
   }
};

} // aedis::detail

#include <boost/asio/unyield.hpp>
#endif // AEDIS_CONNECTION_OPS_HPP
