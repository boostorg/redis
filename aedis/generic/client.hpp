/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vector>
#include <limits>
#include <functional>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/node.hpp>
#include <aedis/generic/detail/client_ops.hpp>
#include <aedis/redis/command.hpp>

// TODO: What to do if users send a discard command not contained in a
// transaction. The client object will try to pop the queue until a
// multi is found.

namespace aedis {
namespace generic {

/** \brief A high level Redis client.
 *  \ingroup any
 *
 *  This class represents a connection to the Redis server. Some of
 *  its most important features are
 *
 *  1. Automatic management of commands. The implementation will send
 *     commands and read responses automatically for the user.
 *  2. Memory reuse. Dynamic memory allocations will decrease with time.
 *
 *  For more details, please see the documentation of each individual
 *  function.
 */
template <class AsyncReadWriteStream, class Command>
class client {
public:
   /// Executor used.
   using executor_type = typename AsyncReadWriteStream::executor_type;

   /// Type of the callback called on a message is received.
   using read_handler_type = std::function<void(Command cmd, std::size_t)>;

   /// Type of the callback called on a message is written.
   using write_handler_type = std::function<void(std::size_t)>;

   /// Type of the callback called on a push message is received.
   using push_handler_type = std::function<void(std::size_t)>;

   /// Type of the callback called when parts of a message are received.
   using resp3_handler_type = std::function<void(Command, resp3::node<boost::string_view> const&, boost::system::error_code&)>;

   using default_completion_token_type = boost::asio::default_completion_token_t<executor_type>;

   struct config {
      /// Timeout to read a complete message.
      std::chrono::seconds read_timeout = std::chrono::seconds{5};

      /// Timeout to write a message.
      std::chrono::seconds write_timeout = std::chrono::seconds{5};

      /// Timeout of the connect operation.
      std::chrono::seconds connect_timeout = std::chrono::seconds{5};

      /// The maximum size of a read.
      std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)();
   };

   /** \brief Constructor.
    *
    *  \param ex The executor.
    */
   client(boost::asio::any_io_executor ex, config cfg = config{})
   : socket_{ex}
   , read_timer_{ex}
   , write_timer_{ex}
   , wait_write_timer_{ex}
   , cfg_{cfg}
   , on_connect_ {[this]() {send(Command::hello, 3);}}
   {
   }

   /// Returns the executor.
   auto get_executor() {return socket_.get_executor();}

   /** @brief Adds a command to the output command queue.
    *
    *  Adds a command to the output command queue and signals the write
    *  operation there are new messages awaiting to be sent to Redis.
    *
    *  @sa serializer.hpp
    *
    *  @param cmd The command to send.
    *  @param args Arguments to commands.
    */
   template <class... Ts>
   void send(Command cmd, Ts const&... args)
   {
      auto const can_write = prepare_next();

      serializer<std::string> sr(requests_);
      auto const before = requests_.size();
      sr.push(cmd, args...);
      auto const after = requests_.size();
      assert(after - before != 0);
      info_.back().size += after - before;;

      if (!has_push_response(cmd)) {
         commands_.push_back(cmd);
         ++info_.back().cmds;
      }

      if (can_write)
         wait_write_timer_.cancel_one();
   }

   /** @brief Adds a command to the output command queue.
    *
    *  Adds a command to the output command queue and signals the write
    *  operation there are new messages awaiting to be sent to Redis.
    *
    *  @sa serializer.hpp
    *
    *  @param cmd The command.
    *  @param key The key the commands refers to
    *  @param begin Begin of the range.
    *  @param end End of the range.
    */
   template <class Key, class ForwardIterator>
   void send_range2(Command cmd, Key const& key, ForwardIterator begin, ForwardIterator end)
   {
      if (begin == end)
         return;

      auto const can_write = prepare_next();

      serializer<std::string> sr(requests_);
      auto const before = requests_.size();
      sr.push_range2(cmd, key, begin, end);
      auto const after = requests_.size();
      assert(after - before != 0);
      info_.back().size += after - before;;

      if (!has_push_response(cmd)) {
         commands_.push_back(cmd);
         ++info_.back().cmds;
      }

      if (can_write)
         wait_write_timer_.cancel_one();
   }

   /** @brief Adds a command to the output command queue.
    *
    *  Adds a command to the output command queue and signals the write
    *  operation there are new messages awaiting to be sent to Redis.
    *
    *  @sa serializer.hpp
    *
    *  @param cmd The command.
    *  @param begin Begin of the range.
    *  @param end End of the range.
    */
   template <class ForwardIterator>
   void send_range2(Command cmd, ForwardIterator begin, ForwardIterator end)
   {
      if (begin == end)
         return;

      auto const can_write = prepare_next();

      serializer<std::string> sr(requests_);
      auto const before = requests_.size();
      sr.push_range2(cmd, begin, end);
      auto const after = requests_.size();
      assert(after - before != 0);
      info_.back().size += after - before;;

      if (!has_push_response(cmd)) {
         commands_.push_back(cmd);
         ++info_.back().cmds;
      }

      if (can_write)
         wait_write_timer_.cancel_one();
   }

   /** @brief Adds a command to the output command queue.
    *
    *  Adds a command to the output command queue and signals the write
    *  operation there are new messages awaiting to be sent to Redis.
    *
    *  @sa serializer.hpp
    *
    *  @param cmd The command.
    *  @param key The key the commands refers to.
    *  @param range Range of elements to send.
    */
   template <class Key, class Range>
   void send_range(Command cmd, Key const& key, Range const& range)
   {
      using std::begin;
      using std::end;
      send_range2(cmd, key, begin(range), end(range));
   }

   /** @brief Adds a command to the output command queue.
    *
    *  Adds a command to the output command queue and signals the write
    *  operation there are new messages awaiting to be sent to Redis.
    *
    *  @sa serializer.hpp
    *
    *  @param cmd The command.
    *  @param range End of the range.
    */
   template <class Range>
   void send_range(Command cmd, Range const& range)
   {
      using std::begin;
      using std::end;
      send_range2(cmd, begin(range), end(range));
   }

   /** @brief Starts communication with the Redis server asynchronously.
    *
    *  This class performs the following steps
    *
    *  @li Connect to the endpoint passed in the function parameter.
    *  @li Start the async read operation that keeps reading responses to commands and server pushes.
    *  @li Start the async write operation that keeps sending commands to Redis.
    *
    *  \param recv The receiver (see below)
    *  \param ep The address of the Redis server.
    *  \param token The completion token (ASIO jargon)
    *
    *  The receiver is a class that privides the following member functions
    *
    *  @code
    *  class receiver {
    *  public:
    *     // Called when a new chunck of user data becomes available.
    *     void on_resp3(command cmd, node<boost::string_view> const& nd, boost::system::error_code& ec);
    *
    *     // Called when a response becomes available.
    *     void on_read(command cmd);
    *
    *     // Called when a request has been writen to the socket.
    *     void on_write(std::size_t n);
    *
    *     // Called when a server push is received.
    *     void on_push();
    *  };
    *  @endcode
    *
    */
   template <class CompletionToken = default_completion_token_type>
   auto
   async_run(
      boost::asio::ip::tcp::endpoint ep = {boost::asio::ip::make_address("127.0.0.1"), 6379},
      CompletionToken token = CompletionToken{})
   {
      endpoint_ = ep;
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::run_op<client>{this}, token, socket_, read_timer_, write_timer_, wait_write_timer_);
   }

   void set_read_handler(read_handler_type rh)
      { on_read_ = std::move(rh); }

   void set_write_handler(write_handler_type wh)
      { on_write_ = std::move(wh); }

   void set_push_handler(push_handler_type ph)
      { on_push_ = std::move(ph); }

   void set_resp3_handler(resp3_handler_type rh)
      { on_resp3_ = std::move(rh); }

private:
   // Type of the callback that is called on success connect.
   using connect_handler_type = std::function<void()>;

   template <class T, class U, class V> friend struct detail::reader_op;
   template <class T, class U, class V> friend struct detail::read_op;
   template <class T, class U> friend struct detail::writer_op;
   template <class T, class U> friend struct detail::write_op;
   template <class T, class U> friend struct detail::run_op;

   /* Prepares the back of the queue to receive further commands. 
    *
    * If true is returned the request in the front of the queue can be
    * sent to the server. See async_write_some.
    */
   bool prepare_next()
   {
      if (info_.empty()) {
         info_.push_back({});
         return true;
      }

      if (info_.front().size == 0) {
         // It has already been written and we are waiting for the
         // responses.
         info_.push_back({});
         return false;
      }

      return false;
   }

   // Returns true when the next request can be writen.
   bool on_cmd(Command cmd)
   {
      // TODO: If the response to a discard is received we have to
      // remove all commands up until multi.

      assert(!info_.empty());
      assert(!commands_.empty());

      commands_.erase(std::begin(commands_));

      if (--info_.front().cmds != 0)
         return false;

      info_.erase(std::begin(info_));

      return !info_.empty();
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   async_read(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::read_op<client>{this}, token, socket_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   reader(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::reader_op<client, Command>{this}, token, socket_);
   }

   // Write with a timeout.
   template <class CompletionToken = default_completion_token_type>
   auto
   async_write(
      CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::write_op<client>{this}, token, socket_, write_timer_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   writer(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::writer_op<client>{this}, token, socket_, wait_write_timer_);
   }

   struct info {
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
   std::vector<Command> commands_;

   // Info about the requests.
   std::vector<info> info_;

   // The stream.
   AsyncReadWriteStream socket_;

   // Read operation timer.
   boost::asio::steady_timer read_timer_;

   // Writer timer.
   boost::asio::steady_timer write_timer_;

   // Timer used to inform the write operation that there is a message
   // in the output queue.
   boost::asio::steady_timer wait_write_timer_;

   // Redis endpoint.
   boost::asio::ip::tcp::endpoint endpoint_;

   config cfg_;

   // Callbacks.
   read_handler_type on_read_ = [](Command, std::size_t){};
   write_handler_type on_write_ = [](std::size_t){};
   push_handler_type on_push_ = [](std::size_t){};
   resp3_handler_type on_resp3_ = [](Command, resp3::node<boost::string_view> const&, boost::system::error_code&) {};
   connect_handler_type on_connect_ = []() {};;

   // Some state needed by the operations.
   bool stop_writer_ = false;
   resp3::type data_type = resp3::type::invalid;
   Command cmd = Command::invalid;
};

} // generic
} // aedis
