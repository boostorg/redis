/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <array>

#include <boost/system.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/connect.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/read.hpp>
#include <aedis/generic/error.hpp>

namespace aedis {
namespace generic {
namespace detail {

#include <boost/asio/yield.hpp>

template <class Client>
struct check_idle_op {
   Client* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void
   operator()( Self& self
             , boost::system::error_code ec = {}
             , boost::asio::ip::tcp::resolver::results_type res = {})
   {
      reenter (coro) for(;;) {

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
      reenter (coro) {
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
      reenter (coro) {
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
      reenter (coro) {

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

            default: assert(false);
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

            default: assert(false);
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
      reenter (coro) {

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
              assert(ec1);
              self.complete(ec1);
           } break;
           case 1:
           {
              assert(ec2);
              self.complete(ec2);
           } break;
           case 2:
           {
              assert(ec3);
              self.complete(ec3);
           } break;
           default: assert(false);
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
      reenter (coro) {

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

         assert(false);
      }
   }
};

// Consider limiting the size of the pipelines by spliting that last
// one in two if needed.
template <class Client>
struct write_op {
   Client* cli;
   std::size_t size;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , std::size_t n = 0
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro) {

         assert(!cli->info_.empty());
         assert(cli->info_.front().size != 0);
         assert(!cli->requests_.empty());

         cli->write_timer_.expires_after(cli->cfg_.write_timeout);
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

            default: assert(false);
         }

         assert(n == cli->info_.front().size);
         size = cli->info_.front().size;

         cli->requests_.erase(0, cli->info_.front().size);
         cli->info_.front().size = 0;
         
         if (cli->info_.front().cmds == 0) 
            cli->info_.erase(std::begin(cli->info_));

         cli->on_write_(size);
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
      reenter (coro) for (;;) {
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
      reenter (coro) {
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

            default: assert(false);
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
      reenter (coro) for (;;) {

         boost::ignore_unused(n);

         if (cli->read_buffer_.empty()) {
            yield
            boost::asio::async_read_until(
               *cli->socket_,
               boost::asio::dynamic_buffer(cli->read_buffer_, cli->cfg_.max_read_size),
               "\r\n",
               std::move(self));

            if (ec) {
               cli->on_reader_exit();
               self.complete(ec);
               return;
            }
         }

         assert(!cli->read_buffer_.empty());
         cli->type_ = resp3::to_type(cli->read_buffer_.front());
         cli->cmd_info_ = std::make_pair<>(Command::invalid, 0);
         if (cli->type_ != resp3::type::push) {
            assert(!cli->commands_.empty());
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
