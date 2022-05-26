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
         cli->check_idle_timer_.expires_after(2 * cli->cfg_.ping_delay_timeout);

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

template <class Conn>
struct exec_op {
   Conn* cli;
   typename Conn::request_type const* req;
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
         timer = std::make_shared<boost::asio::steady_timer>(cli->read_timer_.get_executor());
         timer->expires_at(std::chrono::steady_clock::time_point::max());
         cli->add_request(*req, timer);

         // Notice we use the back of the queue.
         yield timer->async_wait(std::move(self));
         if (!cli->socket_->is_open()) {
            // TODO: Pass the correct error.
            self.complete(error::idle_timeout, 0);
            return;
         }

         // Notice we use the front of the queue.
         BOOST_ASSERT(!cli->reqs_.empty());
         while (cli->reqs_.front().n_cmds != 0) {
            yield cli->read_ch_.async_receive(std::move(self));
            if (ec) {
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

         BOOST_ASSERT(cli->reqs_.front().n_cmds == 0);
         cli->reqs_.pop_front(); // TODO: Recycle timers.

         if (!cli->reqs_.empty()) {
            if (cli->n_cmds_ == 0)
               cli->wait_write_timer_.cancel_one();
            else
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
         cli->ping_timer_.expires_after(cli->cfg_.ping_delay_timeout);
         yield cli->ping_timer_.async_wait(std::move(self));
         if (ec) {
            // The timer has been canceled, continue.
            self.complete(ec);
            return;
         }

         // The timer fired, send the ping. If there is an ongoing
         // command there is no need to send a new one.
         if (!cli->reqs_.empty())
            continue;

         cli->req_.clear();
         cli->req_.push(Command::ping);
         yield cli->async_exec(cli->req_, std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }
      }
   }
};

template <class Conn>
struct idle_check_op {
   Conn* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro) for (;;)
      {
         cli->check_idle_timer_.expires_after(2 * cli->cfg_.ping_delay_timeout);
         yield cli->check_idle_timer_.async_wait(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         auto const now = std::chrono::steady_clock::now();
         if (cli->last_data_ +  (2 * cli->cfg_.ping_delay_timeout) < now) {
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
         cli->wait_write_timer_.expires_at(std::chrono::steady_clock::time_point::max());

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
            >(cli->read_timer_.get_executor());

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

         //if (cli->n_cmds_ == 0 && !cli->reqs_.empty()) {
         if (!cli->reqs_.empty()) {
            yield cli->async_write_with_timeout(std::move(self));
            if (ec) {
               cli->close();
               self.complete(ec);
               return;
            }

            cli->reqs_.front().timer->cancel_one();
         }

         yield cli->wait_write_timer_.async_wait(std::move(self));

         if (!cli->socket_->is_open()) {
            self.complete(error::write_stop_requested);
            return;
         }
      }
   }
};

template <class Conn, class Command>
struct read_with_timeout_op {
   Conn* cli;
   Command cmd;
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
         cli->read_timer_.expires_after(cli->cfg_.read_timeout);

         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return resp3::async_read(*cli->socket_, cli->make_dynamic_buffer(), cli->select_adapter(cmd), token);},
            [this](auto token) { return cli->read_timer_.async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

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
                  self.complete(error::read_timeout, 0);
                  return;
               }
            } break;

            default: BOOST_ASSERT(false);
         }

         self.complete({}, n);
      }
   }
};

template <class Conn, class Command>
struct reader_op {
   Conn* cli;
   resp3::type type_ =  resp3::type::invalid;
   Command cmd_ = Command::invalid;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      boost::ignore_unused(n);

      reenter (coro) for (;;)
      {
         if (cli->read_buffer_.empty()) {
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
         }

         // TODO: Treat type::invalid as error.
         // TODO: I noticed that unsolicited simple-error events are
         // Sent by the server (-MISCONF). Send them through the
         // channel. The only way to detect them is check whether the
         // queue is empty.
         BOOST_ASSERT(!cli->read_buffer_.empty());
         type_ = resp3::to_type(cli->read_buffer_.front());
         cmd_ = Command::invalid;
         if (type_ != resp3::type::push) {
            BOOST_ASSERT(!cli->reqs_.empty());
            BOOST_ASSERT(cli->reqs_.front().n_cmds != 0);
            BOOST_ASSERT(!cli->cmds_.empty());
            cmd_ = cli->cmds_.front();
         }

         cli->last_data_ = std::chrono::steady_clock::now();

         yield cli->async_read_with_timeout(cmd_, std::move(self));
         if (ec) {
            cli->close();
            self.complete(ec);
            return;
         }

         if (cmd_ == Command::invalid) {
            yield cli->push_ch_.async_send({}, n, std::move(self));
         } else {
            yield cli->read_ch_.async_send({}, n, std::move(self));
         }

         if (ec) {
            cli->close();
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
