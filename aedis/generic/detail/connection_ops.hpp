/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_GENERIC_CONNECTION_OPS_HPP
#define AEDIS_GENERIC_CONNECTION_OPS_HPP

#include <array>

#include <boost/system.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/connect.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/assert.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/read.hpp>
#include <aedis/resp3/write.hpp>
#include <aedis/generic/error.hpp>
#include <aedis/redis/command.hpp>
#include <aedis/adapter/adapt.hpp>

namespace aedis {
namespace generic {
namespace detail {

#include <boost/asio/yield.hpp>

template <class Conn>
struct exec_internal_impl_op {
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
         yield
         boost::asio::async_write(
            *cli->socket_,
            boost::asio::buffer(req->payload()),
            std::move(self));

         if (ec) {
            self.complete(ec);
            return;
         }

         yield
         resp3::async_read(
            *cli->socket_,
            cli->make_dynamic_buffer(),
            [](resp3::node<boost::string_view> const&, boost::system::error_code&) { },
            std::move(self));

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
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro)
      {
         // Idle timeout.
         cli->check_idle_timer_.expires_after(2 * cli->cfg_.ping_interval);

         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return cli->async_exec_internal_impl(*req, token);},
            [this](auto token) { return cli->check_idle_timer_.async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         switch (order[0]) {
            case 0:
            {
               if (ec1) {
                  self.complete(ec1);
                  return;
               }
            } break;

            case 1:
            {
               if (!ec2) {
                  self.complete(error::idle_timeout);
                  return;
               }
            } break;

            default: BOOST_ASSERT(false);
         }

         self.complete({});
      }
   }
};

template <class Conn, class Command, class Adapter>
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
         //std::cout << "push_op: waiting to process push." << std::endl;
         if (cli->waiting_pushes_ == 0) {
            yield cli->wait_push_timer_.async_wait(std::move(self));
            if (!cli->socket_->is_open()) {
               self.complete(ec, 0);
               return;
            }

            //std::cout << "push_op: After wait." << std::endl;
            BOOST_ASSERT(cli->waiting_pushes_ == 1);
         }


         //std::cout << "push_op: starting to process the push." << std::endl;
         yield
         resp3::async_read(
            *cli->socket_,
            cli->make_dynamic_buffer(),
            adapter,
            std::move(self));

         //std::cout << "push_op: finish, calling the reader_op." << std::endl;
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
   std::shared_ptr<boost::asio::steady_timer> timer;
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
         // TODO: Check first if there is a recycled channel
         // available.
         timer = std::make_shared<boost::asio::steady_timer>(cli->resv_.get_executor());
         timer->expires_at(std::chrono::steady_clock::time_point::max());
         cli->add_request(*req, timer);

         //std::cout << "exec_op: waiting to process a read " << cli->reqs_.size() << std::endl;
         // Notice we use the back of the queue.
         yield timer->async_wait(std::move(self));
         if (!cli->socket_->is_open()) {
            self.complete(ec, 0);
            return;
         }

         BOOST_ASSERT(!cli->reqs_.empty());
         if (cli->reqs_.front().n_cmds == 0) {
            // Some requests don't have response, so we have to exit
            // the operation earlier.
            cli->reqs_.pop_front(); // TODO: Recycle timers.

            // If there is no ongoing push-read operation we can
            // request the timer to proceed, otherwise we can just
            // exit.
            if (cli->waiting_pushes_ == 0) {
               //std::cout << "exec_op: requesting read op to proceed." << std::endl;
               cli->wait_read_timer_.cancel_one();
            }
            self.complete({}, 0);
            return;
         }

         // Notice we use the front of the queue.
         while (cli->reqs_.front().n_cmds != 0) {
            BOOST_ASSERT(!cli->cmds_.empty());
            yield
            resp3::async_read(
               *cli->socket_,
               cli->make_dynamic_buffer(),
               [adpt = adapter, cmd = cli->cmds_.front()] (resp3::node<boost::string_view> const& nd, boost::system::error_code& ec) mutable { adpt(cmd, nd, ec); },
               std::move(self));
            if (ec) {
               cli->close();
               self.complete(ec, 0);
               return;
            }

            read_size += n;

            BOOST_ASSERT(cli->reqs_.front().n_cmds != 0);
            BOOST_ASSERT(cli->n_cmds_ != 0);
            BOOST_ASSERT(!cli->cmds_.empty());

            --cli->reqs_.front().n_cmds;
            --cli->n_cmds_;
            cli->cmds_.pop();
         }

         BOOST_ASSERT(!cli->reqs_.empty());
         BOOST_ASSERT(cli->reqs_.front().n_cmds == 0);
         cli->reqs_.pop_front(); // TODO: Recycle timers.

         if (cli->n_cmds_ == 0) {
            // We are done with the pipeline and can resumes listening
            // on the socket and send pending requests if there is
            // any.
            //std::cout << "exec_op: requesting read op to proceed." << std::endl;
            cli->wait_read_timer_.cancel_one();
            if (!cli->reqs_.empty()) {
               //std::cout << "exec_op: Requesting a write." << std::endl;
               cli->wait_write_timer_.cancel_one();
            }
         } else {
            // We are not done with the pipeline and can continue
            // reading.
            BOOST_ASSERT(!cli->reqs_.empty());
            cli->reqs_.front().timer->cancel_one();
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
         //std::cout << "ping_op: waiting to send a ping." << std::endl;
         cli->ping_timer_.expires_after(cli->cfg_.ping_interval);
         yield cli->ping_timer_.async_wait(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         //std::cout << "ping_op: Sending a command." << std::endl;
         cli->req_.clear();
         cli->req_.push(Command::ping);
         yield cli->async_exec(cli->req_, adapter::adapt(), std::move(self));
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
struct resolve_with_timeout_op {
   Conn* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::asio::ip::tcp::resolver::results_type res = {}
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro)
      {
         // Tries to resolve with a timeout. We can use the writer
         // timer here as there is no ongoing write operation.
         cli->write_timer_.expires_after(cli->cfg_.resolve_timeout);

         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return cli->resv_.async_resolve(cli->cfg_.host.data(), cli->cfg_.port.data(), token);},
            [this](auto token) { return cli->write_timer_.async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         switch (order[0]) {
            case 0:
            {
               if (ec1) {
                  self.complete(ec1);
                  return;
               }
            } break;

            case 1:
            {
               if (!ec2) {
                  self.complete(error::resolve_timeout);
                  return;
               }
            } break;

            default: BOOST_ASSERT(false);
         }

         cli->endpoints_ = res;
         self.complete({});
      }
   }
};

template <class Conn>
struct connect_with_timeout_op {
   Conn* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::asio::ip::tcp::endpoint const& ep = {}
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro)
      {
         // Tries a connection with a timeout. We can use the writer
         // timer here as there is no ongoing write operation.
         cli->write_timer_.expires_after(cli->cfg_.connect_timeout);

         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return boost::asio::async_connect(*cli->socket_, cli->endpoints_, token);},
            [this](auto token) { return cli->write_timer_.async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         switch (order[0]) {
            case 0:
            {
               if (ec1) {
                  self.complete(ec1);
                  return;
               }
            } break;

            case 1:
            {
               if (!ec2) {
                  self.complete(error::connect_timeout);
                  return;
               }
            } break;

            default: BOOST_ASSERT(false);
         }

         self.complete({});
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
         //std::cout << "read_write_check_ping_op: Setting the timer." << std::endl;

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
   boost::asio::coroutine coro;

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro)
      {
         yield cli->async_resolve_with_timeout(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         cli->socket_ =
            std::make_shared<
               typename Conn::next_layer_type
            >(cli->resv_.get_executor());

         yield cli->async_connect_with_timeout(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         cli->req_.clear();
         cli->req_.push(Command::hello, 3);
         yield cli->async_exec_internal(cli->req_, std::move(self));
         if (ec) {
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
struct write_op {
   Conn* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      reenter (coro)
      {
         //std::cout << "write_op: before." << std::endl;
         BOOST_ASSERT(!cli->reqs_.empty());
         BOOST_ASSERT(!cli->payload_next_.empty());

         // Prepare for the next write.
         cli->n_cmds_ = cli->n_cmds_next_;
         cli->n_cmds_next_ = 0;
         cli->payload_ = cli->payload_next_;
         cli->payload_next_.clear();

         yield
         boost::asio::async_write(
            *cli->socket_,
            boost::asio::buffer(cli->payload_),
            std::move(self));
         //std::cout << "write_op: after." << std::endl;

         BOOST_ASSERT(!cli->reqs_.empty());
         if (cli->reqs_.front().n_cmds == 0) {
            // Some requests don't have response, so their timers
            // won't be canceled on read op, we have to do it here.
            cli->reqs_.front().timer->cancel_one();
            // Notice we don't have to call
            // cli->wait_read_timer_.cancel_one(); as that operation
            // is ongoing.
            self.complete({}, n);
            return;
         }

         cli->payload_.clear();
         self.complete(ec, n);
      }
   }
};

template <class Conn>
struct write_with_timeout_op {
   Conn* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , std::size_t n = 0
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro)
      {
         cli->write_timer_.expires_after(cli->cfg_.write_timeout);

         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return cli->async_write(token);},
            [this](auto token) { return cli->write_timer_.async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         //std::cout << "write_with_timeout_op: completed." << std::endl;

         switch (order[0]) {
            case 0:
            {
               if (ec1) {
                  self.complete(ec1, 0);
                  return;
               }
            } break;

            case 1:
            {
               if (!ec2) {
                  self.complete(error::write_timeout, 0);
                  return;
               }
            } break;

            default: BOOST_ASSERT(false);
         }

         self.complete({}, n);
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
         // When cli->cmds_ we are still processing the last request.
         // The timer must be however canceled so we can unblock the
         // channel.

         if (!cli->reqs_.empty()) {
            yield cli->async_write_with_timeout(std::move(self));
            if (ec) {
               cli->close();
               self.complete(ec);
               return;
            }
         }

         //std::cout << "writer_op: waiting to write." << std::endl;
         yield cli->wait_write_timer_.async_wait(std::move(self));
         if (!cli->socket_->is_open()) {
            self.complete(error::write_stop_requested);
            return;
         }

         //std::cout << "writer_op: Write requested: " << ec.message() << std::endl;
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
         //std::cout << "reader_op: waiting data." << std::endl;
         yield
         boost::asio::async_read_until(
            *cli->socket_,
            cli->make_dynamic_buffer(),
            "\r\n",
            std::move(self));
         if (ec) {
            cli->close();
            self.complete(ec);
            return;
         }

         // TODO: Treat type::invalid as error.
         // TODO: I noticed that unsolicited simple-error events are
         // Sent by the server (-MISCONF). Send them through the
         // channel. The only way to detect them is check whether the
         // queue is empty.

         cli->last_data_ = std::chrono::steady_clock::now();
         if (resp3::to_type(cli->read_buffer_.front()) == resp3::type::push) {
            //std::cout << "reader_op: Requesting the push op." << std::endl;
            cli->waiting_pushes_ = 1;
            cli->wait_push_timer_.cancel_one();
         } else {
            //std::cout << "reader_op: Requesting the read op." << std::endl;
            BOOST_ASSERT(!cli->cmds_.empty());
            BOOST_ASSERT(cli->reqs_.front().n_cmds != 0);
            cli->reqs_.front().timer->cancel_one();
         }

         //std::cout << "reader_op: waiting to read." << std::endl;
         cli->wait_read_timer_.expires_after(cli->cfg_.read_timeout);
         yield cli->wait_read_timer_.async_wait(std::move(self));
         //std::cout << "reader_op: after wait: " << ec.message() << std::endl;
         if (!ec) {
            //std::cout << "reader_op: error1." << std::endl;
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
} // generic
} // aedis

#endif // AEDIS_GENERIC_CONNECTION_OPS_HPP
