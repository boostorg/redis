/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_GENERIC_CLIENT_OPS_HPP
#define AEDIS_GENERIC_CLIENT_OPS_HPP

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
#include <aedis/generic/error.hpp>

namespace aedis {
namespace generic {
namespace detail {

#include <boost/asio/yield.hpp>

template <class Client, class Command>
struct ping_after_op {
   Client* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void
   operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro)
      {
         BOOST_ASSERT((cli->cfg_.idle_timeout / 2) != std::chrono::seconds{0});
         cli->read_timer_.expires_after(cli->cfg_.idle_timeout / 2);
         yield cli->read_timer_.async_wait(std::move(self));
         if (ec) {
            BOOST_ASSERT(ec == boost::asio::error::operation_aborted);
            self.complete({}); // Not an error.
            return;
         }

         // The timer fired, send the ping.
         cli->send(Command::ping);
         self.complete({});
      }
   }
};

template <class Client>
struct read_until_op {
   Client* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      reenter (coro)
      {
         // Waits for incomming data.
         yield
         boost::asio::async_read_until(
            *cli->socket_,
            boost::asio::dynamic_buffer(cli->read_buffer_, cli->cfg_.max_read_size),
            "\r\n",
            std::move(self));

         // Cancels the async_ping_after.
         cli->read_timer_.cancel();
         self.complete(ec);
      }
   }
};

template <class Client>
struct wait_for_data_op {
   Client* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro)
      {
         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return cli->async_read_until(token);},
            [this](auto token) { return cli->async_ping_after(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_all(),
            std::move(self));

         switch (order[0]) {
            case 0:
            {
               // Complete regardless of the error.
               self.complete(ec1);
            } break;

            case 1:
            {
               BOOST_ASSERT(!ec2);
               // Don't complete here as we are still waiting data.
               // self.complete({});
            } break;

            default: BOOST_ASSERT(false);
         }
      }
   }
};

template <class Client>
struct check_idle_op {
   Client* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro) for(;;)
      {
         cli->check_idle_timer_.expires_after(cli->cfg_.idle_timeout);
         yield cli->check_idle_timer_.async_wait(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         auto const now = std::chrono::steady_clock::now();
         if (cli->last_data_ +  cli->cfg_.idle_timeout < now) {
            cli->on_reader_exit();
            self.complete(error::idle_timeout);
            return;
         }

         cli->last_data_ = now;
      }
   }
};

template <class Client>
struct resolve_op {
   Client* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , boost::asio::ip::tcp::resolver::results_type res = {})
   {
      reenter (coro)
      {
         yield
         cli->resv_.async_resolve(cli->host_.data(), cli->port_.data(), std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         cli->endpoints_ = res;
         self.complete({});
      }
   }
};

template <class Client>
struct connect_op {
   Client* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , boost::asio::ip::tcp::endpoint const& ep = {})
   {
      reenter (coro)
      {
         yield
         boost::asio::async_connect(
            *cli->socket_,
            cli->endpoints_,
            std::move(self));

         if (ec) {
            self.complete(ec);
            return;
         }

         cli->endpoint_ = ep;
         self.complete({});
      }
   }
};

template <class Client>
struct init_op {
   Client* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro)
      {
         // Tries to resolve with a timeout. We can use the writer
         // timer here as there is no ongoing write operation.
         cli->write_timer_.expires_after(cli->cfg_.resolve_timeout);

         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return cli->async_resolve(token);},
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

               cli->on_resolve();
            } break;

            case 1:
            {
               if (!ec2) {
                  self.complete(generic::error::resolve_timeout);
                  return;
               }
            } break;

            default: BOOST_ASSERT(false);
         }

         // Tries a connection with a timeout. We can use the writer
         // timer here as there is no ongoing write operation.
         cli->write_timer_.expires_after(cli->cfg_.connect_timeout);

         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return cli->async_connect(token);},
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

               cli->on_connect();
            } break;

            case 1:
            {
               if (!ec2) {
                  self.complete(generic::error::connect_timeout);
                  return;
               }
            } break;

            default: BOOST_ASSERT(false);
         }

         self.complete({});
      }
   }
};

template <class Client>
struct read_write_check_op {
   Client* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 3> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {}
                  , boost::system::error_code ec3 = {})
   {
      reenter (coro)
      {
         // Starts the reader and writer ops.
         cli->wait_write_timer_.expires_at(std::chrono::steady_clock::time_point::max());

         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return cli->writer(token);},
            [this](auto token) { return cli->reader(token);},
            [this](auto token) { return cli->async_check_idle(token);}
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
           default: BOOST_ASSERT(false);
         }
      }
   }
};

template <class Client>
struct run_op {
   Client* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro)
      {
         yield cli->async_init(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         yield cli->async_read_write_check(std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         BOOST_ASSERT(false);
      }
   }
};

template <class Client>
struct write_op {
   Client* cli;
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
         BOOST_ASSERT(!cli->info_.empty());
         BOOST_ASSERT(cli->info_.front().size != 0);
         BOOST_ASSERT(!cli->requests_.empty());

         cli->write_timer_.expires_after(cli->cfg_.write_timeout);
         cli->info_.front().sent = true;
         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return boost::asio::async_write(*cli->socket_, boost::asio::buffer(cli->requests_.data(), cli->info_.front().size), token);},
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
                  self.complete(generic::error::write_timeout);
                  return;
               }
            } break;

            default: BOOST_ASSERT(false);
         }

         BOOST_ASSERT(!cli->info_.empty());
         BOOST_ASSERT(cli->info_.front().size != 0);
         BOOST_ASSERT(!cli->requests_.empty());
         BOOST_ASSERT(n == cli->info_.front().size);

         cli->requests_.erase(0, n);
         cli->info_.front().size = 0;
         if (cli->info_.front().cmds == 0) 
            cli->info_.erase(std::begin(cli->info_));

         cli->on_write_(n);
         self.complete({});
      }
   }
};

template <class Client>
struct writer_op {
   Client* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()(Self& self , boost::system::error_code ec = {})
   {
      reenter (coro) for (;;)
      {
         yield cli->async_write(std::move(self));
         if (ec) {
            cli->socket_->close();
            self.complete(ec);
            return;
         }

         yield cli->wait_write_timer_.async_wait(std::move(self));

         if (!cli->socket_->is_open()) {
            self.complete(error::write_stop_requested);
            return;
         }
      }
   }
};

template <class Client>
struct read_op {
   Client* cli;
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
            [this](auto token) { return resp3::async_read(*cli->socket_, boost::asio::dynamic_buffer(cli->read_buffer_, cli->cfg_.max_read_size), [cli_ = cli](resp3::node<boost::string_view> const& nd, boost::system::error_code& ec) mutable {cli_->on_resp3_(cli_->cmd_info_.first, nd, ec);}, token);},
            [this](auto token) { return cli->read_timer_.async_wait(token);}
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
                  self.complete(generic::error::read_timeout);
                  return;
               }
            } break;

            default: BOOST_ASSERT(false);
         }

         if (cli->type_ == resp3::type::push) {
            cli->on_push_(n);
         } else {
            if (cli->on_cmd(cli->cmd_info_))
               cli->wait_write_timer_.cancel_one();

            cli->on_read_(cli->cmd_info_.first, n);
         }

         self.complete({});
      }
   }
};

template <class Client, class Command>
struct reader_op {
   Client* cli;
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
            yield cli->async_wait_for_data(std::move(self));
            if (ec) {
               cli->on_reader_exit();
               self.complete(ec);
               return;
            }
         }

         BOOST_ASSERT(!cli->read_buffer_.empty());
         cli->type_ = resp3::to_type(cli->read_buffer_.front());
         cli->cmd_info_ = std::make_pair(Command::invalid, 0);
         if (cli->type_ != resp3::type::push) {
            BOOST_ASSERT(!cli->commands_.empty());
            cli->cmd_info_ = cli->commands_.front();
         }

         cli->last_data_ = std::chrono::steady_clock::now();

         yield cli->async_read(std::move(self));
         if (ec) {
            cli->on_reader_exit();
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

#endif // AEDIS_GENERIC_CLIENT_OPS_HPP
