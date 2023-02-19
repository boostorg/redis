/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONNECTION_OPS_HPP
#define BOOST_REDIS_CONNECTION_OPS_HPP

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/detail/read.hpp>
#include <boost/redis/request.hpp>
#include <boost/assert.hpp>
#include <boost/system.hpp>
#include <boost/asio/write.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <array>
#include <algorithm>
#include <string_view>

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
         AEDIS_CHECK_OP0(;);

         BOOST_ASIO_CORO_YIELD
         conn->channel_.async_send(system::error_code{}, 0, std::move(self));
         AEDIS_CHECK_OP0(;);

         self.complete({});
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
   asio::coroutine coro{};

   template <class Self>
   void
   operator()( Self& self
             , system::error_code ec = {}
             , std::size_t n = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro)
      {
         // Loop reading the responses to this request.
         BOOST_ASSERT(!conn->reqs_.empty());
         while (cmds != 0) {
            BOOST_ASSERT(conn->is_waiting_response());

            //-----------------------------------
            // If we detect a push in the middle of a request we have
            // to hand it to the push consumer. To do that we need
            // some data in the read bufer.
            if (conn->read_buffer_.empty()) {
               BOOST_ASIO_CORO_YIELD
               asio::async_read_until(
                  conn->next_layer(),
                  conn->make_dynamic_buffer(),
                  "\r\n", std::move(self));
               AEDIS_CHECK_OP1(conn->cancel(operation::run););
            }

            // If the next request is a push we have to handle it to
            // the receive_op wait for it to be done and continue.
            if (resp3::to_type(conn->read_buffer_.front()) == resp3::type::push) {
               BOOST_ASIO_CORO_YIELD
               conn->async_wait_receive(std::move(self));
               AEDIS_CHECK_OP1(conn->cancel(operation::run););
               continue;
            }
            //-----------------------------------

            BOOST_ASIO_CORO_YIELD
            redis::detail::async_read(
               conn->next_layer(),
               conn->make_dynamic_buffer(),
                  [i = index, adpt = adapter] (resp3::basic_node<std::string_view> const& nd, system::error_code& ec) mutable { adpt(i, nd, ec); },
                  std::move(self));

            ++index;

            AEDIS_CHECK_OP1(conn->cancel(operation::run););

            read_size += n;

            BOOST_ASSERT(cmds != 0);
            --cmds;
         }

         self.complete({}, read_size);
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
         AEDIS_CHECK_OP1(;);

         BOOST_ASIO_CORO_YIELD
         redis::detail::async_read(conn->next_layer(), conn->make_dynamic_buffer(), adapter, std::move(self));
         if (ec || is_cancelled(self)) {
            conn->cancel(operation::run);
            conn->cancel(operation::receive);
            self.complete(!!ec ? ec : asio::error::operation_aborted, {});
            return;
         }

         read_size = n;

         BOOST_ASIO_CORO_YIELD
         conn->channel_.async_receive(std::move(self));
         AEDIS_CHECK_OP1(;);

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
         conn->async_exec_read(adapter, conn->reqs_.front()->get_number_of_commands(), std::move(self));
         AEDIS_CHECK_OP1(;);

         read_size = n;

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

template <class Conn>
struct run_op {
   Conn* conn = nullptr;
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
            [this](auto token) { return conn->writer(token);}
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

template <class Conn>
struct writer_op {
   Conn* conn;
   asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , system::error_code ec = {}
                  , std::size_t n = 0)
   {
      ignore_unused(n);

      BOOST_ASIO_CORO_REENTER (coro) for (;;)
      {
         while (conn->coalesce_requests()) {
            BOOST_ASIO_CORO_YIELD
            asio::async_write(conn->next_layer(), asio::buffer(conn->write_buffer_), std::move(self));
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

         BOOST_ASIO_CORO_YIELD
         conn->writer_timer_.async_wait(std::move(self));
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
   asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , system::error_code ec = {}
                  , std::size_t n = 0)
   {
      ignore_unused(n);

      BOOST_ASIO_CORO_REENTER (coro) for (;;)
      {
         BOOST_ASIO_CORO_YIELD
         asio::async_read_until(
            conn->next_layer(),
            conn->make_dynamic_buffer(),
            "\r\n", std::move(self));

         if (ec == asio::error::eof) {
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
            BOOST_ASIO_CORO_YIELD
            conn->async_wait_receive(std::move(self));
         } else {
            BOOST_ASSERT(conn->is_waiting_response());
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

} // boost::redis::detail

#endif // BOOST_REDIS_CONNECTION_OPS_HPP
