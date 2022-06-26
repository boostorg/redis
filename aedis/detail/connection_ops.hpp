/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_CONNECTION_OPS_HPP
#define AEDIS_CONNECTION_OPS_HPP

#include <array>

#include <boost/assert.hpp>
#include <boost/system.hpp>
#include <boost/asio/write.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <aedis/adapt.hpp>
#include <aedis/error.hpp>
#include <aedis/detail/net.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/read.hpp>
#include <aedis/resp3/exec.hpp>
#include <aedis/resp3/write.hpp>
#include <aedis/resp3/request.hpp>

namespace aedis {
namespace detail {

#include <boost/asio/yield.hpp>

template <class Conn>
struct connect_with_timeout_op {
   Conn* conn;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , boost::asio::ip::tcp::endpoint const& ep = {})
   {
      reenter (coro)
      {
         BOOST_ASSERT(conn->socket_ != nullptr);
         conn->ping_timer_.expires_after(conn->cfg_.connect_timeout);
         yield aedis::detail::async_connect(*conn->socket_, conn->ping_timer_, conn->endpoints_, std::move(self));
         self.complete(ec);
      }
   }
};

template <class Conn>
struct hello_op {
   Conn* conn;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      reenter (coro)
      {
         BOOST_ASSERT(conn->socket_ != nullptr);
         conn->req_.clear();
         conn->req_.push(command::hello, 3);
         conn->ping_timer_.expires_after(std::chrono::seconds{2});
         yield resp3::async_exec(*conn->socket_, conn->ping_timer_, conn->req_, adapter::adapt(), conn->make_dynamic_buffer(), std::move(self));
         self.complete(ec);
      }
   }
};

template <class Conn>
struct resolve_with_timeout_op {
   Conn* conn;
   boost::string_view host;
   boost::string_view port;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , boost::asio::ip::tcp::resolver::results_type res = {})
   {
      reenter (coro)
      {
         conn->ping_timer_.expires_after(conn->cfg_.resolve_timeout);
         yield aedis::detail::async_resolve(conn->resv_, conn->ping_timer_, host, port, std::move(self));
         conn->endpoints_ = res;
         self.complete(ec);
      }
   }
};

template <class Conn, class Adapter>
struct read_push_op {
   Conn* conn;
   Adapter adapter;
   std::size_t read_size;
   boost::asio::coroutine coro;

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t n = 0)
   {
      reenter (coro)
      {
         yield conn->push_channel_.async_receive(std::move(self));
         if (ec) {
            self.complete(ec, 0);
            return;
         }

         BOOST_ASSERT(conn->socket_ != nullptr);
         yield resp3::async_read(*conn->socket_, conn->make_dynamic_buffer(), adapter, std::move(self));
         if (ec) {
            conn->push_channel_.cancel();
            self.complete(ec, 0);
            return;
         }

         read_size = n;

         yield conn->push_channel_.async_send({}, 0, std::move(self));
         if (ec) {
            self.complete(ec, 0);
            return;
         }

         self.complete(ec, read_size);
         return;
      }
   }
};

template <class Conn, class Adapter>
struct exec_read_op {
   Conn* conn;
   Adapter adapter;
   std::size_t read_size = 0;
   std::size_t index = 0;
   boost::asio::coroutine coro;

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
         while (conn->reqs_.front()->expects_response()) {
            BOOST_ASSERT(!conn->cmds_.empty());

            //-----------------------------------
            // If we detect a push in the middle of a request we have
            // to hand it to the push consumer. To do that we need
            // some data in the read bufer.
            if (conn->read_buffer_.empty()) {
               BOOST_ASSERT(conn->socket_ != nullptr);
               yield boost::asio::async_read_until(*conn->socket_, conn->make_dynamic_buffer(), "\r\n", std::move(self));
               if (ec) {
                  self.complete(ec, 0);
                  return;
               }
            }

            // If the next request is a push we have to handle it to
            // the read_push_op wait for it to be done and continue.
            if (resp3::to_type(conn->read_buffer_.front()) == resp3::type::push) {
               yield async_send_receive(conn->push_channel_, std::move(self));
               if (ec) {
                  self.complete(ec, 0);
                  return;
               }
            }
            //-----------------------------------

            yield
            resp3::async_read(*conn->socket_, conn->make_dynamic_buffer(),
               [i = index, adpt = adapter, cmd = conn->cmds_.front()] (resp3::node<boost::string_view> const& nd, boost::system::error_code& ec) mutable { adpt(i, cmd, nd, ec); },
               std::move(self));

            ++index;

            if (ec) {
               self.complete(ec, 0);
               return;
            }

            read_size += n;

            BOOST_ASSERT(conn->reqs_.front()->expects_response());
            conn->reqs_.front()->pop();

            BOOST_ASSERT(!conn->cmds_.empty());
            conn->cmds_.pop();
         }

         self.complete({}, read_size);
      }
   }
};

template <class Conn>
struct exec_write_op {
   Conn* conn;
   std::size_t write_size = 0;
   boost::asio::coroutine coro;

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t n = 0)
   {
      reenter (coro)
      {
         conn->coalesce_requests();
         yield boost::asio::async_write(*conn->socket_, boost::asio::buffer(conn->payload_), std::move(self));
         if (ec) {
            self.complete(ec, 0);
            return;
         }

         write_size = n;

         // We have to clear the payload right after the read op in
         // order to to use it as a flag that informs there is no
         // ongoing write.
         conn->payload_.clear();

         // After the reader receives the response to the command
         // above it will wait untill we signal we are done with the
         // write by writing in the channel.
         yield conn->read_channel_.async_receive(std::move(self));
         self.complete(ec, write_size);
         return;
      }
   }
};

template <class Conn>
struct exec_exit_op {
   Conn* conn;
   boost::asio::coroutine coro;

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t n = 0)
   {
      reenter (coro)
      {
         BOOST_ASSERT(!conn->reqs_.empty());
         conn->release_req_info(conn->reqs_.front());
         conn->reqs_.pop_front();

         if (!conn->reqs_.empty())
            conn->reqs_.front()->timer.cancel_one();

         // Handles control of the read operation to the reader. There
         // are two reasons for that
         //
         // 1. There may be no enqueued command and we have to
         // continuously listen on the socket.
         //
         // 2. There are enqueued commands that will be written, while
         // we are writing we have to listen for messages. e.g. server
         // pushes.
         if (conn->cmds_.empty()) {
            yield conn->read_channel_.async_send({}, 0, std::move(self));
         }

         self.complete(ec);
         return;
      }
   }
};

template <class Conn, class Adapter>
struct exec_op {
   Conn* conn;
   resp3::request const* req;
   Adapter adapter;
   std::shared_ptr<typename Conn::req_info> info;
   std::size_t read_size = 0;
   boost::asio::coroutine coro;

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t n = 0)
   {
      reenter (coro)
      {
         if (!conn->add_request(*req)) {
            // There is an ongoing request being processed, when the
            // response to this specific request arrives, the timer
            // below will be canceled, either in the end of this
            // operation (if it is in the middle of a pipeline) or on
            // the reader_op (if it is the first in the pipeline).
            // Notice we use the back of the queue.
            info = conn->reqs_.back();
            yield info->timer.async_wait(std::move(self));
            if (info->stop) {
               BOOST_ASSERT(!!ec);
               conn->release_req_info(info);
               self.complete(ec, 0);
               return;
            }
         }

         BOOST_ASSERT(!conn->reqs_.empty());
         if (conn->cmds_.empty() && conn->payload_.empty()) {
            yield conn->async_exec_write(std::move(self));
            if (ec) {
               conn->close();
               self.complete(ec, 0);
               return;
            }
         }

         BOOST_ASSERT(!conn->reqs_.empty());
         if (conn->reqs_.front()->expects_response()) {
            yield conn->async_exec_read(adapter, std::move(self));
            if (ec) {
               conn->close();
               self.complete(ec, 0);
               return;
            }

            read_size = n;
            BOOST_ASSERT(!conn->reqs_.empty());
            BOOST_ASSERT(!conn->reqs_.front()->expects_response());
         }

         yield conn->async_exec_exit(std::move(self));
         self.complete(ec, read_size);
      }
   }
};

template <class Conn>
struct ping_op {
   Conn* conn;
   boost::asio::coroutine coro;

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t read_size = 0)
   {
      reenter (coro) for (;;)
      {
         conn->ping_timer_.expires_after(conn->cfg_.ping_interval);
         yield conn->ping_timer_.async_wait(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         conn->req_.clear();
         conn->req_.push(command::ping);
         yield conn->async_exec(conn->req_, aedis::adapt(), std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }
      }
   }
};

template <class Conn>
struct check_idle_op {
   Conn* conn;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro) for (;;)
      {
         conn->check_idle_timer_.expires_after(2 * conn->cfg_.ping_interval);
         yield conn->check_idle_timer_.async_wait(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         auto const now = std::chrono::steady_clock::now();
         if (conn->last_data_ +  (2 * conn->cfg_.ping_interval) < now) {
            conn->close();
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
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 3> order = {}
                  , boost::system::error_code ec0 = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro)
      {
         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return conn->reader(token);},
            [this](auto token) { return conn->async_check_idle(token);},
            [this](auto token) { return conn->async_ping(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one_error(),
            std::move(self));

         switch (order[0]) {
           case 0:
           {
              BOOST_ASSERT(ec0);
              self.complete(ec0);
           } break;
           case 1:
           {
              BOOST_ASSERT(ec1);
              self.complete(ec1);
           } break;
           case 2:
           {
              BOOST_ASSERT(ec2);
              self.complete(ec2);
           } break;
           default: BOOST_ASSERT(false);
         }
      }
   }
};

template <class Conn>
struct run_op {
   Conn* conn;
   boost::string_view host;
   boost::string_view port;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro)
      {
         yield conn->async_resolve_with_timeout(host, port, std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         conn->socket_ = std::make_shared<typename Conn::next_layer_type>(conn->resv_.get_executor());

         yield conn->async_connect_with_timeout(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         yield conn->async_hello(std::move(self));
         if (ec) {
            conn->close();
            self.complete(ec);
            return;
         }

         if (!conn->reqs_.empty())
            conn->reqs_.front()->timer.cancel_one();

         yield conn->async_start(std::move(self));
         self.complete(ec);
      }
   }
};

template <class Conn>
struct reader_op {
   Conn* conn;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      boost::ignore_unused(n);

      reenter (coro) for (;;)
      {
         BOOST_ASSERT(conn->socket_->is_open());
         yield boost::asio::async_read_until(*conn->socket_, conn->make_dynamic_buffer(), "\r\n", std::move(self));
         if (ec) {
            conn->close();
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
             || (!conn->reqs_.empty() && !conn->reqs_.front()->expects_response())) {
            yield async_send_receive(conn->push_channel_, std::move(self));
         } else {
            BOOST_ASSERT(!conn->cmds_.empty());
            BOOST_ASSERT(conn->reqs_.front()->expects_response());
            yield async_send_receive(conn->read_channel_, std::move(self));
         }

         if (ec) {
            self.complete(ec);
            return;
         }
      }
   }
};

template <class Conn, class Adapter>
struct runexec_op {
   Conn* conn;
   boost::string_view host;
   boost::string_view port;
   resp3::request const* req;
   Adapter adapter;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {}
                  , std::size_t n = 0)
   {
      reenter (coro)
      {
         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return conn->async_run(host, port, token);},
            [this](auto token) { return conn->async_exec(*req, adapter, token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one_error(),
            std::move(self));

         if (ec2) {
            self.complete(ec2, n);
         } else {
            // If there was no error in the async_exec we complete
            // with the async_run error, if any.
            self.complete(ec1, n);
         }
      }
   }
};

#include <boost/asio/unyield.hpp>

} // detail
} // aedis

#endif // AEDIS_CONNECTION_OPS_HPP
