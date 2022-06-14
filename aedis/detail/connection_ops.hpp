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
         yield aedis::detail::async_connect(*conn->socket_, conn->write_timer_, conn->endpoints_, std::move(self));
         self.complete(ec);
      }
   }
};

template <class Conn>
struct exec_internal_op {
   Conn* cli;
   resp3::request const* req;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      reenter (coro)
      {
         yield resp3::async_exec( *cli->socket_, cli->check_idle_timer_, *req, adapter::adapt(), cli->make_dynamic_buffer(), std::move(self));
         self.complete(ec);
      }
   }
};

template <class Conn>
struct resolve_with_timeout_op {
   Conn* cli;
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
         yield
         aedis::detail::async_resolve(cli->resv_, cli->write_timer_, host, port, std::move(self));
         cli->endpoints_ = res;
         self.complete(ec);
      }
   }
};

template <class Conn, class Adapter>
struct read_push_op {
   Conn* cli;
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
         yield cli->push_channel_.async_receive(std::move(self));
         if (ec) {
            self.complete(ec, 0);
            return;
         }

         BOOST_ASSERT(cli->socket_ != nullptr);
         yield
         resp3::async_read(*cli->socket_, cli->make_dynamic_buffer(), adapter, std::move(self));
         if (ec) {
            cli->push_channel_.cancel();
            self.complete(ec, 0);
            return;
         }

         read_size = n;

         yield
         cli->push_channel_.async_send(boost::system::error_code{}, 0, std::move(self));
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
struct read_next_op {
   Conn* cli;
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
         BOOST_ASSERT(!cli->reqs_.empty());
         while (cli->reqs_.front()->n_cmds != 0) {
            BOOST_ASSERT(!cli->cmds_.empty());

            //-----------------------------------
            // Section to handle pushes in the middle of a request.
            if (cli->read_buffer_.empty()) {
               // Read in some data.
               yield boost::asio::async_read_until(*cli->socket_, cli->make_dynamic_buffer(), "\r\n", std::move(self));
               if (ec) {
                  cli->close();
                  self.complete(ec, 0);
                  return;
               }
            }

            // If the next request is a push we have to handle it to
            // the read_push_op wait for it to be done and continue.
            if (resp3::to_type(cli->read_buffer_.front()) == resp3::type::push) {
               yield async_send_receive(cli->push_channel_, std::move(self));
               if (ec) {
                  cli->read_channel_.cancel();
                  self.complete(ec, 0);
                  return;
               }
            }
            //-----------------------------------

            yield
            resp3::async_read(
               *cli->socket_,
               cli->make_dynamic_buffer(),
               [i = index, adpt = adapter, cmd = cli->cmds_.front()] (resp3::node<boost::string_view> const& nd, boost::system::error_code& ec) mutable { adpt(i, cmd, nd, ec); },
               std::move(self));

            ++index;

            if (ec) {
               cli->close();
               self.complete(ec, 0);
               return;
            }

            read_size += n;

            BOOST_ASSERT(cli->reqs_.front()->n_cmds != 0);
            BOOST_ASSERT(!cli->cmds_.empty());

            --cli->reqs_.front()->n_cmds;
            cli->cmds_.pop();
         }

         self.complete({}, read_size);
      }
   }
};

template <class Conn, class Adapter>
struct exec_op {
   Conn* cli;
   resp3::request const* req;
   Adapter adapter;
   std::shared_ptr<typename Conn::req_info> info;
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
         // add_request will add the request payload to the buffer and
         // return true if it can be written to the socket i.e. There
         // is no ongoing request.
         if (cli->add_request(*req)) {
            // We can proceeed and write the request to the socket.
            info = cli->reqs_.back();
         } else {
            // There is an ongoing request being processed, when the
            // response to this specific request arrives, the timer
            // below will be canceled, either in the end of this
            // operation (if it is in the middle of a pipeline) or on
            // the reader_op (if it is the first in the pipeline).
            // Notice we use the back of the queue.
            info = cli->reqs_.back();
            yield info->timer.async_wait(std::move(self));
            if (info->stop) {
               cli->release_req_info(info);
               self.complete(boost::asio::error::basic_errors::operation_aborted, 0);
               return;
            }
         }

         //----------------------------------------------------------------
         // Write operation.

         BOOST_ASSERT(!cli->reqs_.empty());

         if (cli->cmds_.empty() && cli->payload_.empty()) {
            // If get here there is no request being processed (so we
            // can write). Otherwise, the payload corresponding to
            // this request has already been sent in previous
            // pipelines and there is nothing to send.
            BOOST_ASSERT(!cli->payload_next_.empty());

            // Copies the request to variable that won't be touched
            // while async_write is suspended.
            std::swap(cli->cmds_next_, cli->cmds_);
            std::swap(cli->payload_next_, cli->payload_);
            cli->cmds_next_ = {};
            cli->payload_next_.clear();

            cli->write_timer_.expires_after(cli->cfg_.write_timeout);
            yield aedis::detail::async_write(*cli->socket_, cli->write_timer_, boost::asio::buffer(cli->payload_), std::move(self));
            if (ec) {
               cli->close();
               self.complete(ec, 0);
               return;
            }

            // A stop may have been requested while the write
            // operation was suspended.
            if (info->stop) {
               self.complete(boost::asio::error::basic_errors::operation_aborted, 0);
               return;
            }

            cli->payload_.clear();

            // Waits for the reader to receive the response. Notice
            // we cannot skip this step as between and async_write and
            // async_read we may receive a server push.
            yield cli->read_channel_.async_receive(std::move(self));
            if (ec) {
               self.complete(ec, 0);
               return;
            }

            if (info->stop) {
               cli->read_channel_.cancel();
               cli->release_req_info(info);
               self.complete(ec, 0);
               return;
            }
         }

         // If the connection we have just written has no expected
         // response e.g. subscribe, the operation has to be
         // completed here.
         BOOST_ASSERT(!cli->reqs_.empty());
         if (cli->reqs_.front()->n_cmds == 0) {
            cli->release_req_info(info);
            cli->reqs_.pop_front();

            if (!cli->reqs_.empty())
               cli->reqs_.front()->timer.cancel_one();

            if (cli->cmds_.empty()) {
               // Done with the pipeline, handles read control to the reader op.
               yield cli->read_channel_.async_send(boost::system::error_code{}, 0, std::move(self));
               if (ec) {
                  self.complete(ec, 0);
                  return;
               }
            }

            self.complete({}, 0);
            return;
         }

         //----------------------------------------------------------------

         yield cli->async_read_next(adapter, std::move(self));
         if (ec) {
            self.complete(ec, 0);
            return;
         }

         BOOST_ASSERT(!cli->reqs_.empty());
         BOOST_ASSERT(cli->reqs_.front()->n_cmds == 0);
         cli->release_req_info(info);
         cli->reqs_.pop_front();

         if (!cli->reqs_.empty())
            cli->reqs_.front()->timer.cancel_one();

         if (cli->cmds_.empty()) {
            // Done with the pipeline, handles read control to the reader op.
            yield cli->read_channel_.async_send(boost::system::error_code{}, 0, std::move(self));
            if (ec) {
               self.complete(ec, 0);
               return;
            }
         }

         self.complete({}, read_size);
         return;
      }
   }
};

template <class Conn>
struct ping_op {
   Conn* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t read_size = 0)
   {
      reenter (coro) for (;;)
      {
         cli->ping_timer_.expires_after(cli->cfg_.ping_interval);
         yield cli->ping_timer_.async_wait(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         cli->req_.clear();
         cli->req_.push(command::ping);
         yield cli->async_exec(cli->req_, aedis::adapt(), std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }
      }
   }
};

template <class Conn>
struct check_idle_op {
   Conn* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro) for (;;)
      {
         cli->check_idle_timer_.expires_after(2 * cli->cfg_.ping_interval);
         yield cli->check_idle_timer_.async_wait(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         auto const now = std::chrono::steady_clock::now();
         if (cli->last_data_ +  (2 * cli->cfg_.ping_interval) < now) {
            cli->close();
            self.complete(error::idle_timeout);
            return;
         }

         cli->last_data_ = now;
      }
   }
};

template <class Conn>
struct read_write_check_ping_op {
   Conn* cli;
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
            [this](auto token) { return cli->reader(token);},
            [this](auto token) { return cli->async_idle_check(token);},
            [this](auto token) { return cli->async_ping(token);}
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
   Conn* cli;
   boost::string_view host;
   boost::string_view port;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro)
      {
         cli->write_timer_.expires_after(cli->cfg_.resolve_timeout);
         yield cli->async_resolve_with_timeout(host, port, std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         cli->socket_ =
            std::make_shared<
               typename Conn::next_layer_type
            >(cli->resv_.get_executor());

         cli->write_timer_.expires_after(cli->cfg_.connect_timeout);
         yield cli->async_connect_with_timeout(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         cli->req_.clear();
         cli->req_.push(command::hello, 3);
         cli->check_idle_timer_.expires_after(2 * cli->cfg_.ping_interval);
         yield cli->async_exec_internal(cli->req_, std::move(self));
         if (ec) {
            cli->close();
            self.complete(ec);
            return;
         }

         if (!cli->reqs_.empty()) {
            cli->reqs_.front()->timer.cancel_one();
         }

         yield cli->async_read_write_check_ping(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         BOOST_ASSERT(false);
      }
   }
};

template <class Conn>
struct reader_op {
   Conn* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      boost::ignore_unused(n);

      reenter (coro) for (;;)
      {
         yield boost::asio::async_read_until(*cli->socket_, cli->make_dynamic_buffer(), "\r\n", std::move(self));
         if (ec) {
            cli->close();
            self.complete(ec);
            return;
         }

         cli->last_data_ = std::chrono::steady_clock::now();

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
         BOOST_ASSERT(!cli->read_buffer_.empty());
         if (resp3::to_type(cli->read_buffer_.front()) == resp3::type::push
             || cli->reqs_.empty()
             || (!cli->reqs_.empty() && cli->reqs_.front()->n_cmds == 0)) {
            yield async_send_receive(cli->push_channel_, std::move(self));
            if (ec) {
               self.complete(ec);
               return;
            }
         } else {
            BOOST_ASSERT(!cli->cmds_.empty());
            BOOST_ASSERT(cli->reqs_.front()->n_cmds != 0);

            yield async_send_receive(cli->read_channel_, std::move(self));
            if (ec) {
               self.complete(ec);
               return;
            }
         }

         if (!cli->socket_->is_open()) {
            cli->close();
            self.complete(ec);
            return;
         }
      }
   }
};

template <class Conn, class Adapter>
struct runexec_op {
   Conn* cli;
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
            [this](auto token) { return cli->async_run(host, port, token);},
            [this](auto token) { return cli->async_exec(*req, adapter, token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one_error(),
            std::move(self));

         if (!ec2) {
            // If there was no error in the async_exec we complete
            // with the async_run error, if any.
            self.complete(ec1, n);
            return;
         }
      }
   }
};

#include <boost/asio/unyield.hpp>

} // detail
} // aedis

#endif // AEDIS_CONNECTION_OPS_HPP
