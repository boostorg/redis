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
      /// Timeout of the resolve operation.
      std::chrono::seconds resolve_timeout = std::chrono::seconds{5};

      /// Timeout of the connect operation.
      std::chrono::seconds connect_timeout = std::chrono::seconds{5};

      /// Timeout to read a complete message.
      std::chrono::seconds read_timeout = std::chrono::seconds{5};

      /// Timeout to write a message.
      std::chrono::seconds write_timeout = std::chrono::seconds{5};

      /// The maximum size of a read.
      std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)();
   };

   /** \brief Constructor.
    *
    *  \param ex The executor.
    *  \param cfg Configuration parameters.
    */
   client(boost::asio::any_io_executor ex, config cfg = config{})
   : socket_{ex}
   , read_timer_{ex}
   , write_timer_{ex}
   , wait_write_timer_{ex}
   , resv_{ex}
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
    *  @li Resolves the Redis host as of \c async_resolve with the
    *  timeout passed in config::resolve_timeout.
    *
    *  @li Connects to one of the endpoints returned by the resolve
    *  operation with the timeout passed in config::connect_timeout.
    *
    *  @li Starts the async read operation that keeps reading
    *  incomming responses. Each individual read uses the timeout
    *  passed on config::read_timeout. Calls the read or push callback
    *  after every successful read, see set_read_handler and
    *  set_push_handler.
    *
    *  @li Start the async write operation that keeps sending commands
    *  to Redis. Each individual write uses the timeout passed on
    *  config::write_timeout. Calls the write callback after every
    *  successful write.
    *
    *  \param host Ip address of name of the Redis server.
    *  \param port Port where the Redis server is listening.
    *  \param token The completion token.
    *
    *  The completion token must have the following signature
    *
    *  @code
    *  void f(boost::system::error_code);
    *  @endcode
    */
   template <class CompletionToken = default_completion_token_type>
   auto
   async_run(
      boost::string_view host = "127.0.0.1",
      boost::string_view port = "6379",
      CompletionToken token = CompletionToken{})
   {
      host_ = host;
      port_ = port;

      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::run_op<client>{this}, token, socket_, read_timer_, write_timer_, wait_write_timer_);
   }

   /// Set the read handler.
   void set_read_handler(read_handler_type rh)
      { on_read_ = std::move(rh); }

   /// Set the write handler.
   void set_write_handler(write_handler_type wh)
      { on_write_ = std::move(wh); }

   /// Set the push handler.
   void set_push_handler(push_handler_type ph)
      { on_push_ = std::move(ph); }

   /// Set the resp3 handler.
   void set_resp3_handler(resp3_handler_type rh)
      { on_resp3_ = std::move(rh); }

private:
   // Type of the callback that is called on success connect.
   using connect_handler_type = std::function<void()>;

   template <class T, class V> friend struct detail::reader_op;
   template <class T> friend struct detail::read_op;
   template <class T> friend struct detail::writer_op;
   template <class T> friend struct detail::write_op;
   template <class T> friend struct detail::run_op;
   template <class T> friend struct detail::connect_op;
   template <class T> friend struct detail::resolve_op;

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
         assert(info_.front().cmds != 0);
         // It has already been written and we are waiting for the
         // responses.
         info_.push_back({});
         return false;
      }

      // When cmds = 0 there are only commands with push response on
      // the request and we are not waiting for any response.
      return info_.front().cmds == 0;
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

   // Resolves the address passed in async_run and store the results
   // in endpoints_.
   template <class CompletionToken = default_completion_token_type>
   auto
   async_resolve(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::resolve_op<client>{this}, token, socket_);
   }

   // Connects the socket to one of the endpoints in endpoints_ and
   // stores the successful enpoint in endpoint_.
   template <class CompletionToken = default_completion_token_type>
   auto
   async_connect(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::connect_op<client>{this}, token, socket_);
   }

   // Reads a complete resp3 response from the socket using the
   // timeout config::read_timeout.  On a successful read calls
   // on_read_ or on_push_ depending on whether the response is a push
   // or a response to a command.
   template <class CompletionToken = default_completion_token_type>
   auto
   async_read(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::read_op<client>{this}, token, socket_);
   }

   // Loops on async_read described above.
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

   void on_reader_exit()
   {
      socket_.close();
      wait_write_timer_.expires_at(std::chrono::steady_clock::now());
   }

   // Stores information about a request.
   struct info {
      // Request size in bytes.
      std::size_t size = 0;

      // The number of commands it contains. Commands with push
      // responses are not counted.
      std::size_t cmds = 0;
   };

   // Buffer used by the read operations.
   std::string read_buffer_;

   // Requests payload.
   std::string requests_;

   // The commands contained in the requests.
   std::vector<Command> commands_;

   // Info about the requests.
   std::vector<info> info_;

   // The tcp socket.
   AsyncReadWriteStream socket_;

   // Timer used with async_read.
   boost::asio::steady_timer read_timer_;

   // Timer used with async_write.
   boost::asio::steady_timer write_timer_;

   // Timer that is canceled when a new message is added to the output
   // queue.
   boost::asio::steady_timer wait_write_timer_;

   // Used to resolve the host on async_resolve.
   boost::asio::ip::tcp::resolver resv_;

   // Configuration parameters.
   config cfg_;

   // Called when a complete message is read.
   read_handler_type on_read_ = [](Command, std::size_t){};

   // Called when a request has been written to the socket.
   write_handler_type on_write_ = [](std::size_t){};

   // Called when a complete push message is received.
   push_handler_type on_push_ = [](std::size_t){};

   // Called by the parser after each new chunck of resp3 data is
   // processed.
   resp3_handler_type on_resp3_ = [](Command, resp3::node<boost::string_view> const&, boost::system::error_code&) {};

   // Called when a connection is stablished.
   connect_handler_type on_connect_ = []() {};;

   // Used by the read_op.
   resp3::type data_type = resp3::type::invalid;

   // Used by the read_op.
   Command cmd = Command::invalid;

   // See async_connect.
   boost::asio::ip::tcp::endpoint endpoint_;

   // See async_resolve.
   boost::asio::ip::tcp::resolver::results_type endpoints_;

   // Host passed to async_run.
   boost::string_view host_;

   // Port passed to async_run.
   boost::string_view port_;
};

} // generic
} // aedis
