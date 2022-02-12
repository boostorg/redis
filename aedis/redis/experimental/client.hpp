/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <queue>
#include <functional>

#include <aedis/aedis.hpp>
#include <aedis/redis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/adapt.hpp>

#include <boost/asio/async_result.hpp>

namespace aedis {
namespace redis {
namespace experimental {

inline
auto adapt()
{
   return [](command, resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec) { };
}

template <class T>
auto adapt(T& t)
{
   return [adapter = resp3::adapt(t)](command, resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec) mutable
      { return adapter(t, aggregate_size, depth, data, size, ec); };
}

struct extended_ignore_adapter {
   void operator()(redis::command, resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec) {}
};

/**  \brief A high level redis client.
 *   \ingroup any
 *
 *   This Redis client keeps a connection to the database open and
 *   uses it for all communication with Redis. For examples on how to
 *   use see the examples chat_room.cpp, echo_server.cpp and redis_client.cpp.
 *
 *   \remarks This class reuses its internal buffers for requests and
 *   for reading Redis responses. With time it will allocate less and
 *   less.
 */
class client : public std::enable_shared_from_this<client> {
public:
   /// The type of the socket used by the client.
   //using socket_type = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
   using socket_type = net::ip::tcp::socket;

private:
   template <class T>
   friend struct read_op;

   struct request_info {
      // Request size in bytes.
      std::size_t size = 0;

      // The number of commands it contains excluding commands that
      // have push types as responses, see has_push_response.
      std::size_t cmds = 0;
   };

   // Buffer used in the read operations.
   std::string read_buffer_;

   // Requests payload.
   std::string requests_;

   // The commands contained in the requests.
   std::queue<redis::command> commands_;

   // Info about the requests.
   std::queue<request_info> req_info_;

   // The stream.
   socket_type socket_;

   // Timer used to inform the write coroutine that it can write the
   // next message in the output queue.
   net::steady_timer timer_;

   // Set when the writer coroutine should stop.
   bool stop_writer_ = false;

   /* Prepares the back of the queue to receive further commands. 
    *
    * If true is returned the request in the front of the queue can be
    * sent to the server. See async_write_some.
    */
   bool prepare_next();

public:
   /** \brief Client constructor.
    *
    *  Constructos the client from an executor.
    *
    *  \param ex The executor.
    */
   client(net::any_io_executor ex);

   /// Returns the executor used for I/O with Redis.
   auto get_executor() {return socket_.get_executor();}

   void set_stream(socket_type socket)
   {
      socket_ = std::move(socket);

      net::co_spawn(
          socket_.get_executor(),
          [self = shared_from_this()] { return self->writer(); },
          net::detached);
   }

   // Write coroutine. It is kept suspended until there are messages
   // to be sent.
   net::awaitable<void> writer();

   /** \brief Adds a command to the command queue.
    *
    *  \sa serializer.hpp
    */
   template <class... Ts>
   void send(redis::command cmd, Ts const&... args);

   // Reads messages asynchronously.
   template <
     class ExtendedAdapter = extended_ignore_adapter,
     class CompletionToken = net::use_awaitable_t<>
   >
   auto
   async_read(
      ExtendedAdapter extended_adapter = extended_ignore_adapter{},
      CompletionToken&& token = net::use_awaitable_t<>{});

   // TODO: can we use cancellation.
   void stop_writer()
   {
     stop_writer_ = true;
     timer_.cancel();
     socket_.close();
   }
};

template <class... Ts>
void client::send(redis::command cmd, Ts const&... args)
{
   auto const can_write = prepare_next();

   auto sr = redis::make_serializer(requests_);
   auto const before = std::size(requests_);
   sr.push(cmd, args...);
   auto const after = std::size(requests_);
   assert(after - before != 0);
   req_info_.front().size += after - before;;

   if (!has_push_response(cmd)) {
      commands_.emplace(cmd);
      ++req_info_.front().cmds;
   }

   if (can_write)
      timer_.cancel_one();
}

#include <boost/asio/yield.hpp>

template <class ExtendedAdapter>
struct read_op {
   client* cli;
   ExtendedAdapter adapter;
   net::coroutine coro;
   resp3::type t = resp3::type::invalid;
   redis::command cmd = redis::command::unknown;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      reenter (coro) {
         boost::ignore_unused(n);

         if (ec) {
            cli->stop_writer(); // TODO: implement as cancellation.
            // TODO: close the socket?
            self.complete(ec, redis::command::unknown);
            return;
         }

         if (std::empty(cli->read_buffer_)) {
            yield
            net::async_read_until(
               cli->socket_,
               net::dynamic_buffer(cli->read_buffer_),
               "\r\n",
               std::move(self));

            if (ec) {
               cli->stop_writer();
               // TODO: close the socket?
               self.complete(ec, redis::command::unknown);
               return;
            }
         }

         assert(!std::empty(cli->read_buffer_));
         t = resp3::detail::to_type(cli->read_buffer_.front());
         if (t != resp3::type::push) {
            assert(!std::empty(cli->commands_));
            cmd = cli->commands_.front();
         }

         yield
         resp3::async_read(
            cli->socket_,
            net::dynamic_buffer(cli->read_buffer_),
            [a = adapter, c = cmd](resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec) mutable {a(c, t, aggregate_size, depth, data, size, ec);},
            std::move(self));

         if (ec) {
            cli->stop_writer();
            // TODO: close the socket?
            self.complete(ec, redis::command::unknown);
            return;
         }

         if (t != resp3::type::push) {
            assert(!std::empty(cli->req_info_));
            cli->commands_.pop();
            if (--cli->req_info_.front().cmds == 0) {
               cli->req_info_.pop();
               if (!std::empty(cli->req_info_)) {
                  assert(!std::empty(cli->requests_));
                  yield
                  net::async_write(
                     cli->socket_,
                     net::buffer(cli->requests_.data(), cli->req_info_.front().size),
                     std::move(self));

                  if (ec) {
                     cli->stop_writer();
                     // TODO: close the socket?
                     self.complete(ec, redis::command::unknown);
                     return;
                  }

                  cli->requests_.erase(0, cli->req_info_.front().size);
                  cli->req_info_.front().size = 0;

                  if (cli->req_info_.front().cmds == 0)
                     cli->req_info_.pop();
               }
            }
         }

         self.complete({}, cmd);
      }
   }
};

#include <boost/asio/unyield.hpp>

template <class ExtendedAdapter, class CompletionToken>
auto client::async_read(ExtendedAdapter adapter, CompletionToken&& token)
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code, redis::command)
      >(read_op<ExtendedAdapter>{this, adapter}, token, socket_);
}

} // experimental
} // redis
} // aedis
