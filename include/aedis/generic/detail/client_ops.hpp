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

namespace aedis {
namespace generic {

#include <boost/asio/yield.hpp>

template <class Client, class Receiver>
struct run_op {
   Client* cli;
   Receiver* recv_;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro) {
         yield cli->socket_.async_connect(cli->endpoint_, std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         yield cli->async_read_write(recv_, std::move(self));
         self.complete(ec);
      }
   }
};

template <class Client, class Receiver>
struct read_write_op {
   Client* cli;
   Receiver* recv;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {}
                  )
   {
      reenter (coro) {

         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token) { return cli->async_writer(recv, token);},
            [this](auto token) { return cli->async_reader(recv, token);}
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
template <class Client, class Receiver>
struct writer_op {
   Client* cli;
   Receiver* recv;
   std::size_t size;
   boost::asio::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      reenter (coro) for (;;) {

         boost::ignore_unused(n);

         assert(!cli->req_info_.empty());
         assert(cli->req_info_.front().size != 0);
         assert(!cli->requests_.empty());

         yield
         boost::asio::async_write(
            cli->socket_,
            boost::asio::buffer(cli->requests_.data(), cli->req_info_.front().size),
            std::move(self));

         if (ec) {
            cli->socket_.close();
            self.complete(ec);
            return;
         }

         size = cli->req_info_.front().size;

         cli->requests_.erase(0, cli->req_info_.front().size);
         cli->req_info_.front().size = 0;
         
         if (cli->req_info_.front().cmds == 0) 
            cli->req_info_.erase(std::begin(cli->req_info_));

         recv->on_write(size);

         yield cli->timer_.async_wait(std::move(self));

         if (cli->stop_writer_) {
            self.complete(ec);
            return;
         }
      }
   }
};

template <class Client, class Receiver, class Command>
struct read_op {
   Client* cli;
   Receiver* recv;
   boost::asio::coroutine coro;

   // Consider moving this variables to the client to spare some
   // memory in the competion handler.
   resp3::type t = resp3::type::invalid;
   Command cmd = Command::invalid;

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
               boost::asio::dynamic_buffer(cli->read_buffer_),
               "\r\n",
               std::move(self));

            if (ec) {
               cli->stop_writer_ = true;
               self.complete(ec);
               return;
            }
         }

         assert(!cli->read_buffer_.empty());
         t = resp3::detail::to_type(cli->read_buffer_.front());
         cmd = Command::invalid;
         if (t != resp3::type::push) {
            assert(!cli->commands_.empty());
            cmd = cli->commands_.front();
         }

         yield
         resp3::async_read(
            cli->socket_,
            boost::asio::dynamic_buffer(cli->read_buffer_),
            [p = recv, c = cmd](resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, boost::system::error_code& ec) mutable {p->on_resp3(c, t, aggregate_size, depth, data, size, ec);},
            std::move(self));

         if (ec) {
            cli->stop_writer_ = true;
            self.complete(ec);
            return;
         }

         if (t == resp3::type::push) {
            recv->on_push();
         } else {
            if (cli->on_cmd(cmd))
               cli->timer_.cancel_one();

            recv->on_read(cmd);
         }
      }
   }
};

#include <boost/asio/unyield.hpp>

} // generic
} // aedis
