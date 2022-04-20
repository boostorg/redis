/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <array>

#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/system.hpp>
#include <boost/asio/write.hpp>
#include <boost/core/ignore_unused.hpp>

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/read.hpp>
#include <aedis/generic/error.hpp>

namespace aedis {
namespace generic {
namespace detail {

#include <boost/asio/yield.hpp>

template <class Client>
struct run_op {
   Client* cli;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro) {

         // Tries a connection with a timeout. We can use the writer
         // timer here as there is no ongoing write operation.
         cli->write_timer_.expires_after(cli->cfg_.connect_timeout);
         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return cli->socket_.async_connect(cli->endpoint_, token);},
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
               cli->on_connect_();
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

         // Starts the reader and writer ops.
         cli->wait_write_timer_.expires_at(std::chrono::steady_clock::time_point::max());
         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return cli->writer(token);},
            [this](auto token) { return cli->reader(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one_error(),
            std::move(self));

         switch (order[0]) {
           case 0: self.complete(ec1); break;
           case 1: self.complete(ec2); break;
           default: assert(false);
         }
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
            [this](auto token) { return boost::asio::async_write(cli->socket_, boost::asio::buffer(cli->requests_.data(), cli->info_.front().size), token);},
            [this](auto token) { return cli->write_timer_.async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         switch (order[0]) {
            case 0:
            {
               if (ec1) {
                  cli->socket_.close();
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
            cli->socket_.close();
            self.complete(ec);
            return;
         }

         yield cli->wait_write_timer_.async_wait(std::move(self));

         if (cli->stop_writer_) {
            self.complete(ec);
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
            [this](auto token) { return resp3::async_read(cli->socket_, boost::asio::dynamic_buffer(cli->read_buffer_, cli->cfg_.max_read_size), [cli_ = cli](resp3::node<boost::string_view> const& nd, boost::system::error_code& ec) mutable {cli_->on_resp3_(cli_->cmd, nd, ec);}, token);},
            [this](auto token) { return cli->read_timer_.async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         switch (order[0]) {
            case 0:
            {
               if (ec1) {
                  cli->socket_.close();
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

         if (cli->data_type == resp3::type::push) {
            cli->on_push_(n);
         } else {
            if (cli->on_cmd(cli->cmd))
               cli->wait_write_timer_.cancel_one();

            cli->on_read_(cli->cmd, n);
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
               cli->socket_,
               boost::asio::dynamic_buffer(cli->read_buffer_, cli->cfg_.max_read_size),
               "\r\n",
               std::move(self));

            if (ec) {
               cli->stop_writer_ = true;
               self.complete(ec);
               return;
            }
         }

         assert(!cli->read_buffer_.empty());
         cli->data_type = resp3::to_type(cli->read_buffer_.front());
         cli->cmd = Command::invalid;
         if (cli->data_type != resp3::type::push) {
            assert(!cli->commands_.empty());
            cli->cmd = cli->commands_.front();
         }

         yield cli->async_read(std::move(self));
         if (ec) {
            cli->stop_writer_ = true;
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
