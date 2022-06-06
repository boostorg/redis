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
   typename Conn::request_type const* req;
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

template <class Conn, class Adapter, class Command>
struct read_push_op {
   Conn* cli;
   Adapter adapter;
   boost::asio::coroutine coro;

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , std::size_t n = 0)
   {
      reenter (coro)
      {
         if (cli->waiting_pushes_ == 0) {
            yield cli->wait_push_timer_.async_wait(std::move(self));
            if (!cli->socket_->is_open()) {
               self.complete(ec, 0);
               return;
            }

            BOOST_ASSERT(cli->waiting_pushes_ == 1);
         }

         yield
         resp3::async_read(
            *cli->socket_,
            cli->make_dynamic_buffer(),
            [adpt = adapter](resp3::node<boost::string_view> const& n, boost::system::error_code& ec) mutable { adpt(std::size_t(-1), Command::invalid, n, ec);},
            std::move(self));

         cli->wait_read_timer_.cancel_one();
         cli->waiting_pushes_ = 0;
         self.complete(ec, n);
         return;
      }
   }
};

template <class Conn, class Adapter>
struct exec_op {
   Conn* cli;
   typename Conn::request_type const* req;
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
         cli->add_request(*req);
         // Notice we use the back of the queue.
         info = cli->reqs_.back();
         info->timer.expires_at(std::chrono::steady_clock::time_point::max());
         yield info->timer.async_wait(std::move(self));
         if (info->stop) {
            cli->release_req_info(info);
            self.complete(ec, 0);
            return;
         }

         BOOST_ASSERT(!cli->reqs_.empty());
         if (cli->reqs_.front()->n_cmds == 0) {
            // Some requests don't have response, so we have to exit
            // the operation earlier.
            cli->release_req_info(info);
            cli->reqs_.pop_front();

            // If there is no ongoing push-read operation we can
            // request the reader to proceed, otherwise we can just
            // exit.
            if (cli->waiting_pushes_ == 0)
               cli->wait_read_timer_.cancel_one();

            self.complete({}, 0);
            return;
         }

         // Notice we use the front of the queue.
         while (cli->reqs_.front()->n_cmds != 0) {
            BOOST_ASSERT(!cli->cmds_.empty());

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
            BOOST_ASSERT(cli->n_cmds_ != 0);
            BOOST_ASSERT(!cli->cmds_.empty());

            --cli->reqs_.front()->n_cmds;
            --cli->n_cmds_;
            cli->cmds_.pop();
         }

         BOOST_ASSERT(!cli->reqs_.empty());
         BOOST_ASSERT(cli->reqs_.front()->n_cmds == 0);
         cli->release_req_info(info);
         cli->reqs_.pop_front();

         if (cli->n_cmds_ == 0) {
            // We are done with the pipeline and can resumes listening
            // on the socket and send pending requests if there is
            // any.
            cli->wait_read_timer_.cancel_one();
            if (!cli->reqs_.empty()) {
               cli->wait_write_timer_.cancel_one();
            }
         } else {
            // We are not done with the pipeline and can continue
            // reading.
            BOOST_ASSERT(!cli->reqs_.empty());
            cli->reqs_.front()->timer.cancel_one();
         }

         self.complete({}, read_size);
         return;
      }
   }
};

template <class Conn, class Command>
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
         cli->req_.push(Command::ping);
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
                  , std::array<std::size_t, 4> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {}
                  , boost::system::error_code ec3 = {}
                  , boost::system::error_code ec4 = {})
   {
      reenter (coro)
      {
         // Starts the reader and writer ops.

         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return cli->writer(token);},
            [this](auto token) { return cli->reader(token);},
            [this](auto token) { return cli->async_idle_check(token);},
            [this](auto token) { return cli->async_ping(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one_error(),
            std::move(self));

         switch (order[0]) {
           case 0:
           {
              BOOST_ASSERT(ec1);
              self.complete(ec1);
           } break;
           case 1:
           {
              BOOST_ASSERT(ec2);
              self.complete(ec2);
           } break;
           case 2:
           {
              BOOST_ASSERT(ec3);
              self.complete(ec3);
           } break;
           case 3:
           {
              BOOST_ASSERT(ec4);
              self.complete(ec4);
           } break;
           default: BOOST_ASSERT(false);
         }
      }
   }
};

template <class Conn, class Command>
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
         cli->req_.push(Command::hello, 3);
         cli->check_idle_timer_.expires_after(2 * cli->cfg_.ping_interval);
         yield cli->async_exec_internal(cli->req_, std::move(self));
         if (ec) {
            cli->close();
            self.complete(ec);
            return;
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
struct writer_op {
   Conn* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void
   operator()(Self& self,
              boost::system::error_code ec = {},
              std::size_t n = 0)
   {
      reenter (coro) for (;;)
      {
         if (!cli->reqs_.empty()) {
            BOOST_ASSERT(!cli->reqs_.empty());
            BOOST_ASSERT(!cli->payload_next_.empty());

            // Prepare for the next write.
            cli->n_cmds_ = cli->n_cmds_next_;
            cli->n_cmds_next_ = 0;
            cli->payload_ = cli->payload_next_;
            cli->payload_next_.clear();

            cli->write_timer_.expires_after(cli->cfg_.write_timeout);
            yield aedis::detail::async_write(
               *cli->socket_, cli->write_timer_, boost::asio::buffer(cli->payload_),
               std::move(self));
            if (ec) {
               cli->close();
               self.complete(ec);
               return;
            }

            cli->payload_.clear();
            BOOST_ASSERT(!cli->reqs_.empty());
            if (cli->reqs_.front()->n_cmds == 0) {
               // Some requests don't have response, so their timers
               // won't be canceled on read op, we have to do it here.
               cli->reqs_.front()->timer.cancel_one();
               // Notice we don't have to call
               // cli->wait_read_timer_.cancel_one(); as that
               // operation is ongoing.
            }
         }

         yield cli->wait_write_timer_.async_wait(std::move(self));
         if (!cli->socket_->is_open()) {
            // The completion has been explicited requested.
            self.complete({});
            return;
         }
      }
   }
};

template <class Conn, class Command>
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
         yield boost::asio::async_read_until(
            *cli->socket_,
            cli->make_dynamic_buffer(),
            "\r\n",
            std::move(self));
         if (ec) {
            cli->close();
            self.complete(ec);
            return;
         }

         cli->last_data_ = std::chrono::steady_clock::now();

         if (resp3::to_type(cli->read_buffer_.front()) == resp3::type::push) {
            cli->waiting_pushes_ = 1;
            cli->wait_push_timer_.cancel_one();
         } else if (cli->reqs_.empty()) {
            // This situation is odd. I have noticed that unsolicited
            // simple-error events are sent by the server (-MISCONF)
            // under certain conditions. I expect them to have type
            // push so we can distinguish them from responses to
            // commands, but it is a simple-error. If we are lucky
            // enough to receive them when the command queue is empty
            // we can treat them as server pushes, otherwise it is
            // impossible to handle them properly.
            cli->waiting_pushes_ = 1;
            cli->wait_push_timer_.cancel_one();
         } else {
            BOOST_ASSERT(!cli->cmds_.empty());
            BOOST_ASSERT(cli->reqs_.front()->n_cmds != 0);
            cli->reqs_.front()->timer.cancel_one();
         }

         cli->wait_read_timer_.expires_after(cli->cfg_.read_timeout);
         yield cli->wait_read_timer_.async_wait(std::move(self));
         if (!ec) {
            self.complete(error::read_timeout);
            return;
         }

         if (!cli->socket_->is_open()) {
            self.complete(ec);
            return;
         }
      }
   }
};

#include <boost/asio/unyield.hpp>

} // detail
} // aedis

#endif // AEDIS_CONNECTION_OPS_HPP
