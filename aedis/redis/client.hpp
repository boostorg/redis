/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vector>
#include <aedis/resp3/type.hpp>
#include <aedis/redis/detail/client_ops.hpp>
#include <aedis/redis/command.hpp>

// TODO: What to do if users send a discard command not contained in a
// transaction. The client object will try to pop the queue until a
// multi is found.

namespace aedis {
namespace redis {

/**  \brief A high level redis client.
 *   \ingroup any
 *
 *   This Redis client keeps a connection to the database open and
 *   uses it for all communication with Redis. For examples on how to
 *   use see the examples chat_room.cpp, echo_server.cpp and
 *   redis_client.cpp.
 *
 *   \remarks This class reuses its internal buffers for requests and
 *   for reading Redis responses. With time it will allocate less and
 *   less.
 */
template <class AsyncReadWriteStream>
class client {
public:
   using stream_type = AsyncReadWriteStream;
   using executor_type = stream_type::executor_type;
   using default_completion_token_type = net::default_completion_token_t<executor_type>;

private:
   template <class T, class U> friend struct read_op;
   template <class T, class U> friend struct writer_op;
   template <class T, class U> friend struct read_write_op;
   template <class T, class U> friend struct run_op;

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
   bool on_cmd(command)
   {
      // TODO: If the response to a discard is received we have to
      // remove all commands up until multi.

      assert(!std::empty(req_info_));
      assert(!std::empty(commands_));

      commands_.erase(std::begin(commands_));

      if (--req_info_.front().cmds != 0)
         return false;

      req_info_.erase(std::begin(req_info_));

      return !std::empty(req_info_);
   }

   // Reads messages asynchronously.
   template <
      class Receiver,
      class CompletionToken = default_completion_token_type>
   auto
   async_reader(
      Receiver* recv,
      CompletionToken&& token = default_completion_token_type{})
   {
      return net::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(read_op<client, Receiver>{this, recv}, token, socket_);
   }

   template <
      class Receiver,
      class CompletionToken = default_completion_token_type>
   auto
   async_writer(
      Receiver* recv,
      CompletionToken&& token = default_completion_token_type{})
   {
      return net::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(writer_op<client, Receiver>{this, recv}, token, socket_, timer_);
   }

   template <
      class Receiver,
      class CompletionToken = default_completion_token_type>
   auto
   async_read_write(
      Receiver* recv,
      CompletionToken&& token = default_completion_token_type{})
   {
      return net::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(read_write_op<client, Receiver>{this, recv}, token, socket_, timer_);
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
      send(command::hello, 3);
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

   /** \brief Sends and iterator range (overload with key).
    */
   template <class Key, class ForwardIterator>
   void send_range2(command cmd, Key const& key, ForwardIterator begin, ForwardIterator end)
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

   /** \brief Sends and iterator range (overload without key).
    */
   template <class ForwardIterator>
   void send_range2(command cmd, ForwardIterator begin, ForwardIterator end)
   {
      if (begin == end)
         return;

      auto const can_write = prepare_next();

      auto sr = redis::make_serializer(requests_);
      auto const before = std::size(requests_);
      sr.push_range(cmd, begin, end);
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

   /** \brief Sends a range.
    */
   template <class Key, class Range>
   void send_range(command cmd, Key const& key, Range const& range)
   {
      using std::begin;
      using std::end;
      send_range2(cmd, key, begin(range), end(range));
   }

   /** \brief Sends a range.
    */
   template <class Range>
   void send_range(command cmd, Range const& range)
   {
      using std::begin;
      using std::end;
      send_range2(cmd, begin(range), end(range));
   }

   /** \brief Starts communication with the Redis server asynchronously.
    */
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
      endpoint_ = ep;
      return net::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(run_op<client, Receiver>{this, &recv}, token, socket_, timer_);
   }
};

} // redis
} // aedis
