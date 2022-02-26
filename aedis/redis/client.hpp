/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vector>
#include <array>
#include <functional>
#include <variant>
#include <tuple>

#include <boost/mp11.hpp>

#include <aedis/aedis.hpp>
#include <aedis/redis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/adapt.hpp>
#include <aedis/resp3/response_traits.hpp>

#include <boost/asio/experimental/parallel_group.hpp>

namespace aedis {
namespace redis {

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

#include <boost/asio/yield.hpp>

template <class Client>
struct run_op {
   Client* cli;
   net::coroutine coro;

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro) {
         yield cli->socket_.async_connect(cli->endpoint_, std::move(self));
         if (ec) {
            self.complete(ec);
            return;
         }

         cli->send(command::hello, 3);
         yield cli->async_read_write(std::move(self));
         self.complete(ec);
      }
   }
};

template <class Client>
struct read_write_op {
   Client* cli;
   net::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {}
                  )
   {
      reenter (coro) {

         yield
         net::experimental::make_parallel_group(
            [this](auto token) { return cli->async_writer(token);},
            [this](auto token) { return cli->async_reader(token);}
         ).async_wait(
            net::experimental::wait_for_one_error(),
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
struct writer_op {
   Client* cli;
   std::size_t size;
   net::coroutine coro;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      reenter (coro) for (;;) {

         boost::ignore_unused(n);

         assert(!std::empty(cli->req_info_));
         assert(cli->req_info_.front().size != 0);
         assert(!std::empty(cli->requests_));

         yield
         net::async_write(
            cli->socket_,
            net::buffer(cli->requests_.data(), cli->req_info_.front().size),
            std::move(self));

         if (ec) {
            // TODO: Close the socket so that the reader can return.
            self.complete(ec);
            return;
         }

         size = cli->req_info_.front().size;

         cli->requests_.erase(0, cli->req_info_.front().size);
         cli->req_info_.front().size = 0;
         
         if (cli->req_info_.front().cmds == 0) 
            cli->req_info_.erase(std::begin(cli->req_info_));

         cli->on_write_(size);

         yield cli->timer_.async_wait(std::move(self));

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
   net::coroutine coro;
   resp3::type t = resp3::type::invalid;
   redis::command cmd = redis::command::unknown;

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      reenter (coro) for (;;) {

         boost::ignore_unused(n);

         if (std::empty(cli->read_buffer_)) {
            yield
            net::async_read_until(
               cli->socket_,
               net::dynamic_buffer(cli->read_buffer_),
               "\r\n",
               std::move(self));

            if (ec) {
               cli->stop_writer_ = true;
               self.complete(ec);
               return;
            }
         }

         assert(!std::empty(cli->read_buffer_));
         t = resp3::detail::to_type(cli->read_buffer_.front());
         cmd = redis::command::unknown;
         if (t != resp3::type::push) {
            assert(!std::empty(cli->commands_));
            cmd = cli->commands_.front();
         }

         yield
         resp3::async_read(
            cli->socket_,
            net::dynamic_buffer(cli->read_buffer_),
            [p = cli, c = cmd](resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec) mutable {p->response_adapter_(c, t, aggregate_size, depth, data, size, ec);},
            std::move(self));

         if (ec) {
            cli->stop_writer_ = true;
            self.complete(ec);
            return;
         }

         if (t != resp3::type::push && cli->on_cmd())
            cli->timer_.cancel_one();

         cli->on_read_(cmd);
      }
   }
};
#include <boost/asio/unyield.hpp>

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
template <class AsyncReadWriteStream>
class client {
public:
   /// The type of the socket used by the client.
   //using stream_type = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::socket>;
   using stream_type = AsyncReadWriteStream;
   using executor_type = stream_type::executor_type;
   using default_completion_token_type = net::default_completion_token_t<executor_type>;
   using writer_callback_type = std::function<void(std::size_t)>;
   using reader_callback_type =  std::function<void(command)>;

private:
   using response_adapter_type = std::function<void(command, resp3::type, std::size_t, std::size_t, char const*, std::size_t, std::error_code&)>;

   template <class T>
   friend struct read_op;

   template <class T>
   friend struct writer_op;

   template <class T>
   friend struct read_write_op;

   template <class T>
   friend struct run_op;

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
   std::vector<redis::command> commands_;

   // Info about the requests.
   std::vector<request_info> req_info_;

   // The stream.
   stream_type socket_;

   // Timer used to inform the write coroutine that it can write the
   // next message in the output queue.
   net::steady_timer timer_;

   // Redis endpoint.
   net::ip::tcp::endpoint endpoint_;

   bool stop_writer_ = false;

   writer_callback_type on_write_ = [](std::size_t){};
   reader_callback_type on_read_ = [](command){};
   response_adapter_type response_adapter_ = [](command, resp3::type, std::size_t, std::size_t, char const*, std::size_t, std::error_code&){};

   /* Prepares the back of the queue to receive further commands. 
    *
    * If true is returned the request in the front of the queue can be
    * sent to the server. See async_write_some.
    */
   bool prepare_next()
   {
      if (std::empty(req_info_)) {
         req_info_.push_back({});
         return true;
      }

      if (req_info_.front().size == 0) {
         // It has already been written and we are waiting for the
         // responses.
         req_info_.push_back({});
         return false;
      }

      return false;
   }

   // Returns true when the next request can be writen.
   bool on_cmd()
   {
      assert(!std::empty(req_info_));
      assert(!std::empty(commands_));

      commands_.erase(std::begin(commands_));

      if (--req_info_.front().cmds != 0)
         return false;

      req_info_.erase(std::begin(req_info_));

      return !std::empty(req_info_);
   }

   // Reads messages asynchronously.
   template <class CompletionToken = default_completion_token_type>
   auto async_reader(CompletionToken&& token = default_completion_token_type{})
   {
      return net::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(read_op<client>{this}, token, socket_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto async_writer(CompletionToken&& token = default_completion_token_type{})
   {
      return net::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(writer_op<client>{this}, token, socket_, timer_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   async_read_write(
      CompletionToken&& token = default_completion_token_type{})
   {
      return net::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(read_write_op<client>{this}, token, socket_, timer_);
   }
public:
   /** \brief Client constructor.
    *
    *  Constructos the client from an executor.
    *
    *  \param ex The executor.
    */
   client(net::any_io_executor ex)
   : socket_{ex}
   , timer_{ex}
   {
      timer_.expires_at(std::chrono::steady_clock::time_point::max());
   }

   /// Returns the executor used for I/O with Redis.
   auto get_executor() {return socket_.get_executor();}

   /** \brief Adds a command to the command queue.
    *
    *  \sa serializer.hpp
    */
   template <class... Ts>
   void send(command cmd, Ts const&... args)
   {
      auto const can_write = prepare_next();

      auto sr = redis::make_serializer(requests_);
      auto const before = std::size(requests_);
      sr.push(cmd, args...);
      auto const after = std::size(requests_);
      assert(after - before != 0);
      req_info_.front().size += after - before;;

      if (!has_push_response(cmd)) {
         commands_.push_back(cmd);
         ++req_info_.front().cmds;
      }

      if (can_write)
         timer_.cancel_one();
   }

   template <class Key, class ForwardIterator>
   void send_range(command cmd, Key const& key, ForwardIterator begin, ForwardIterator end)
   {
      if (begin == end)
         return;

      auto const can_write = prepare_next();

      auto sr = redis::make_serializer(requests_);
      auto const before = std::size(requests_);
      sr.push_range(cmd, key, begin, end);
      auto const after = std::size(requests_);
      assert(after - before != 0);
      req_info_.front().size += after - before;;

      if (!has_push_response(cmd)) {
         commands_.push_back(cmd);
         ++req_info_.front().cmds;
      }

      if (can_write)
         timer_.cancel_one();
   }

   template <
     class Receiver,
     class CompletionToken = default_completion_token_type
   >
   auto
   async_run(
      Receiver& recv,
      net::ip::tcp::endpoint ep = {net::ip::make_address("127.0.0.1"), 6379},
      CompletionToken token = CompletionToken{})
   {
      // To avoid large completion handlers, the objects in the class.
      endpoint_ = ep;
      response_adapter_ = [&recv](command cmd, resp3::type t, std::size_t aggregate_size, std::size_t depth, char const* data, std::size_t size, std::error_code& ec) { recv.on_resp3(cmd, t, aggregate_size, depth, data, size, ec);},
      on_read_ = [&recv](command cmd) { recv.on_read(cmd); };
      on_write_ = [&recv](std::size_t n) { recv.on_write(n); };

      return net::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(run_op<client>{this}, token, socket_, timer_);
   }
};

template <class Tuple, class Callable>
class custom_adapter {
private:
   using tuple_type = Tuple;

   using variant_type =
      boost::mp11::mp_rename<boost::mp11::mp_transform<resp3::response_traits_t, tuple_type>, std::variant>;

   std::array<variant_type, std::tuple_size<tuple_type>::value> adapters_;
   Callable to_tuple_index;

public:
   custom_adapter(tuple_type* r, Callable c)
   : to_tuple_index{c}
   { resp3::adapter::detail::assigner<std::tuple_size<tuple_type>::value - 1>::assign(adapters_, *r); }

   void
   operator()(
      command cmd,
      resp3::type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t size,
      std::error_code& ec)
   {
      auto const i = to_tuple_index(cmd);
      if (i == std::tuple_size<Tuple>::value)
        return;

      std::visit(
         [&](auto& arg){arg(t, aggregate_size, depth, data, size, ec);},
         adapters_[i]);
   }
};

template <class Tuple, class Callable>
auto adapt2(Tuple& t, Callable c)
{
   return custom_adapter<Tuple, Callable>(&t, c);
}

template <class Tuple>
class receiver_base {
private:
   using variant_type = boost::mp11::mp_rename<boost::mp11::mp_transform<resp3::response_traits_t, Tuple>, std::variant>;
   std::array<variant_type, std::tuple_size<Tuple>::value> adapters_;

protected:
   Tuple resps_;
   virtual int to_tuple_index(command cmd) { return 0; }

public:
   receiver_base(Tuple& t)
      { resp3::adapter::detail::assigner<std::tuple_size<Tuple>::value - 1>::assign(adapters_, t); }

   void
   on_resp3(
      command cmd,
      resp3::type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t size,
      std::error_code& ec)
   {
      auto const i = to_tuple_index(cmd);
      if (i == -1)
        return;

      std::visit([&](auto& arg){arg(t, aggregate_size, depth, data, size, ec);}, adapters_[i]);
   }

   virtual void on_read(command) { }
   virtual void on_write(std::size_t) { }
};

} // redis
} // aedis
