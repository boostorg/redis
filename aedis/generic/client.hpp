/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_GENERIC_CLIENT_HPP
#define AEDIS_GENERIC_CLIENT_HPP

#include <vector>
#include <limits>
#include <functional>
#include <iterator>
#include <algorithm>
#include <utility>
#include <chrono>
#include <queue>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/experimental/channel.hpp>

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/node.hpp>
#include <aedis/redis/command.hpp>
#include <aedis/adapter/adapt.hpp>
#include <aedis/generic/request.hpp>
#include <aedis/generic/detail/client_ops.hpp>

// TODO: Don't pass pong to the adapter.

namespace aedis {
namespace generic {

/** \brief A high level Redis client.
 *  \ingroup any
 *
 *  This class keeps a connection open to the Redis server where
 *  commands can be sent at any time. For more details, please see the
 *  documentation of each individual function.
 *
 *  https://redis.io/docs/reference/sentinel-clients
 */
template <class AsyncReadWriteStream, class Command>
class client {
public:
   /// Executor type.
   using executor_type = typename AsyncReadWriteStream::executor_type;

   /// Callback type of resp3 operations.
   using adapter_type = std::function<void(Command, resp3::node<boost::string_view> const&, boost::system::error_code&)>;

   /// resp3 callback type (version without command).
   using adapter_type2 = std::function<void(resp3::node<boost::string_view> const&, boost::system::error_code&)>;

   /// Type of the last layer
   using next_layer_type = AsyncReadWriteStream;

   using default_completion_token_type = boost::asio::default_completion_token_t<executor_type>;

   /** @brief Configuration parameters.
    */
   struct config {
      /// Ip address or name of the Redis server.
      std::string host = "127.0.0.1";

      /// Port where the Redis server is listening.
      std::string port = "6379";

      /// Timeout of the \c async_resolve operation.
      std::chrono::milliseconds resolve_timeout = std::chrono::seconds{5};

      /// Timeout of the \c async_connect operation.
      std::chrono::milliseconds connect_timeout = std::chrono::seconds{5};

      /// Timeout of the \c async_read operation.
      std::chrono::milliseconds read_timeout = std::chrono::seconds{5};

      /// Timeout of the \c async_write operation.
      std::chrono::milliseconds write_timeout = std::chrono::seconds{5};

      /// Time after which a ping is sent if no data is received.
      std::chrono::milliseconds ping_delay_timeout = std::chrono::seconds{5};

      /// The maximum size allwed in a read operation.
      std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)();
   };

   /** \brief Constructor.
    *
    *  \param ex The executor.
    *  \param cfg Configuration parameters.
    */
   client(
      boost::asio::any_io_executor ex,
      adapter_type adapter,
      config cfg = config{})
   : resv_{ex}
   , read_timer_{ex}
   , ping_timer_{ex}
   , write_timer_{ex}
   , wait_write_timer_{ex}
   , check_idle_timer_{ex}
   , read_ch_{ex}
   , push_ch_{ex}
   , cfg_{cfg}
   , adapter_{adapter}
   , last_data_{std::chrono::time_point<std::chrono::steady_clock>::min()}
   , type_{resp3::type::invalid}
   , cmd_info_{std::make_pair<Command>(Command::invalid, 0)}
   {
   }

   client(
      boost::asio::any_io_executor ex,
      adapter_type2 a = adapter::adapt(),
      config cfg = config{})
   : client(
         ex,
         [adapter = std::move(a)] (Command cmd, resp3::node<boost::string_view> const& nd, boost::system::error_code& ec) mutable { if (cmd != Command::ping) adapter(nd, ec); },
         cfg)
   {}

   /// Returns the executor.
   auto get_executor() {return read_timer_.get_executor();}

   /** @brief Starts communication with the Redis server asynchronously.
    *
    *  This function performs the following steps
    *
    *  @li Resolves the Redis host as of \c async_resolve with the
    *  timeout passed in client::config::resolve_timeout.
    *
    *  @li Connects to one of the endpoints returned by the resolve
    *  operation with the timeout passed in client::config::connect_timeout.
    *
    *  @li Starts the \c async_read operation that keeps reading incoming
    *  responses. Each individual read uses the timeout passed on
    *  client::config::read_timeout. After each successful read it
    *  will call the read or push callback.
    *
    *  @li Starts the \c async_write operation that waits for new commands
    *  to be sent to Redis. Each individual write uses the timeout
    *  passed on client::config::write_timeout. After a successful
    *  write it will call the write callback.
    *
    *  @li Starts the idle check operation with the timeout of twice
    *  the value of client::config::ping_delay_timeout. If no data is
    *  received during that time interval \c async_run completes with
    *  generic::error::idle_timeout.
    *
    *  @li Starts the healthy check operation that sends
    *  redis::command::ping to Redis with a frequency equal to
    *  client::config::ping_delay_timeout.
    *
    *  In addition to the callbacks mentioned above, the read
    *  operations will call the resp3 callback as soon a new chunks of
    *  data become available to the user.
    *
    *  It is safe to call \c async_run after it has returned.  In this
    *  case, any outstanding commands will be sent after the
    *  connection is restablished. If a disconnect occurs while the
    *  response to a request has not been received, the client doesn't
    *  try to resend it to avoid resubmission.
    *
    *  Example:
    *
    *  @code
    *  awaitable<void> run_with_reconnect(std::shared_ptr<client_type> db)
    *  {
    *     auto ex = co_await this_coro::executor;
    *     asio::steady_timer timer{ex};
    *  
    *     for (error_code ec;;) {
    *        co_await db->async_run("127.0.0.1", "6379", redirect_error(use_awaitable, ec));
    *        timer.expires_after(std::chrono::seconds{2});
    *        co_await timer.async_wait(redirect_error(use_awaitable, ec));
    *     }
    *  }
    *  @endcode
    *
    *  \param token The completion token.
    *
    *  The completion token must have the following signature
    *
    *  @code
    *  void f(boost::system::error_code);
    *  @endcode
    *
    *  \return This function returns only when there is an error.
    */
   template <class CompletionToken = default_completion_token_type>
   auto async_run(CompletionToken token = CompletionToken{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::run_op<client, Command>{this}, token, read_timer_);
   }

   void add_request(request<Command>& req)
   {
      auto const can_write = prepare_back();
      reqs_.back().req = &req;
      if (can_write)
         wait_write_timer_.cancel_one();
   }

   /** @brief Asynchrnously executes schedules a command for execution.
    */
   template <class CompletionToken = default_completion_token_type>
   auto async_exec(
      request<Command>& req,
      CompletionToken token = CompletionToken{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t, std::size_t)
         >(detail::exec_op<client>{this, &req}, token, read_timer_);
   }

   /** @brief Receives events produced by the run operation.
    */
   template <class CompletionToken = default_completion_token_type>
   auto async_read_push(CompletionToken token = CompletionToken{})
   {
      return push_ch_.async_receive(token);
   }

   /// Set the response adapter.
   void set_adapter(adapter_type adapter)
      { adapter_ = std::move(adapter); }

   /// Set the response adapter.
   void set_adapter(adapter_type2 a)
   {
      adapter_ =
         [adapter = std::move(a)]
            (Command cmd, resp3::node<boost::string_view> const& nd, boost::system::error_code& ec) mutable
         {
            if (cmd != Command::ping)
               adapter(nd, ec);
         };
   }

   /** @brief Closes the connection with the database.
    *  
    *  The channels will be cancelled.
    */
   void close()
   {
      // TODO: Channels should not be canceled if we want to
      // reconnect.
      socket_->close();
      wait_write_timer_.expires_at(std::chrono::steady_clock::now());
      ping_timer_.cancel();
      read_ch_.cancel();
      push_ch_.cancel();
      reqs_ = {};
   }

private:
   using time_point_type = std::chrono::time_point<std::chrono::steady_clock>;
   using read_channel_type = boost::asio::experimental::channel<void(boost::system::error_code, std::size_t)>;
   using write_channel_type = boost::asio::experimental::channel<void(boost::system::error_code, std::size_t)>;
   using request_type = generic::request<Command>;

   template <class T, class V> friend struct detail::reader_op;
   template <class T, class V> friend struct detail::ping_op;
   template <class T> friend struct detail::read_with_timeout_op;
   template <class T> friend struct detail::read_op;
   template <class T, class V> friend struct detail::run_op;
   template <class T> friend struct detail::write_op;
   template <class T> friend struct detail::writer_op;
   template <class T> friend struct detail::write_with_timeout_op;
   template <class T> friend struct detail::connect_op;
   template <class T> friend struct detail::connect_with_timeout_op;
   template <class T> friend struct detail::resolve_op;
   template <class T> friend struct detail::resolve_with_timeout_op;
   template <class T> friend struct detail::check_idle_op;
   template <class T> friend struct detail::read_write_check_ping_op;
   template <class T> friend struct detail::exec_op;

   auto make_dynamic_buffer()
   {
      return boost::asio::dynamic_buffer(read_buffer_, cfg_.max_read_size);
   }

   auto wrap_adapter()
   {
      return [this]
         (resp3::node<boost::string_view> const& nd,
          boost::system::error_code& ec) mutable
         {
            adapter_(cmd_info_.first, nd, ec);
         };
   }

   // Prepares the back of the queue to receive further commands.  If
   // true is returned the request in the front of the queue can be
   // sent to the server.
   //bool prepare_back()
   //{
   //   if (reqs_.empty()) {
   //      reqs_.push({write_channel_type{read_timer_.get_executor()}, nullptr, false});
   //      return true;
   //   }

   //   if (reqs_.front().sent) {
   //      // There is a pending response, we can't modify the front of
   //      // the vector.
   //      BOOST_ASSERT(!reqs_.front().req->commands().empty());
   //      if (reqs_.size() == 1)
   //         reqs_.push({write_channel_type{read_timer_.get_executor()}, nullptr, false});

   //      return false;
   //   }

   //   // When cmds = 0 there are only commands with push response on
   //   // the request and we are not waiting for any response.
   //   return reqs_.front().req->commands().empty();
   //}
   bool prepare_back()
   {
      auto const can_write = reqs_.empty();
      reqs_.push({write_channel_type{read_timer_.get_executor()}, nullptr, false});
      return can_write;
   }

   // Resolves the address passed in async_run and store the results
   // in the endpoints_ member variable.
   template <class CompletionToken = default_completion_token_type>
   auto
   async_resolve(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::resolve_op<client>{this}, token, resv_.get_executor());
   }

   // Calls client::async_resolve with the resolve timeout passed in
   // the config. Uses the write_timer_ to perform the timeout op.
   template <class CompletionToken = default_completion_token_type>
   auto
   async_resolve_with_timeout(
      CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::resolve_with_timeout_op<client>{this},
               token, resv_.get_executor());
   }

   // Connects the socket to one of the endpoints in endpoints_ and
   // stores the successful endpoint in endpoint_.
   template <class CompletionToken = default_completion_token_type>
   auto
   async_connect(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::connect_op<client>{this}, token,
               write_timer_.get_executor());
   }

   // Calls client::async_connect with a timeout.
   template <class CompletionToken = default_completion_token_type>
   auto
   async_connect_with_timeout(
         CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::connect_with_timeout_op<client>{this}, token,
               write_timer_.get_executor());
   }

   // Reads a complete resp3 response from the socket using the
   // timeout config::read_timeout.
   template <class CompletionToken = default_completion_token_type>
   auto
   async_read_with_timeout(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::read_with_timeout_op<client>{this}, token, read_timer_.get_executor());
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   async_read(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::read_op<client>{this}, token, read_timer_.get_executor());
   }

   // Loops on async_read_with_timeout described above.
   template <class CompletionToken = default_completion_token_type>
   auto
   reader(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::reader_op<client, Command>{this}, token, read_timer_.get_executor());
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   async_write(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::write_op<client>{this}, token, write_timer_);
   }

   // Write with a timeout.
   template <class CompletionToken = default_completion_token_type>
   auto
   async_write_with_timeout(
      CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::write_with_timeout_op<client>{this}, token, write_timer_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   writer(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::writer_op<client>{this}, token, wait_write_timer_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   async_read_write_check_ping(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::read_write_check_ping_op<client>{this}, token, read_timer_, write_timer_, wait_write_timer_, check_idle_timer_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   async_ping(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::ping_op<client, Command>{this}, token, read_timer_);
   }

   template <class CompletionToken = default_completion_token_type>
   auto
   async_check_idle(CompletionToken&& token = default_completion_token_type{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::check_idle_op<client>{this}, token, check_idle_timer_);
   }

   // Used to resolve the host on async_resolve.
   boost::asio::ip::tcp::resolver resv_;

   // The tcp socket.
   std::shared_ptr<AsyncReadWriteStream> socket_;

   // Timer used with async_read.
   boost::asio::steady_timer read_timer_;

   // Ping timer.
   boost::asio::steady_timer ping_timer_;

   // Timer used with async_write_with_timeout.
   boost::asio::steady_timer write_timer_;

   // Timer that is canceled when a new message is added to the output
   // queue.
   boost::asio::steady_timer wait_write_timer_;

   // Check idle timer.
   boost::asio::steady_timer check_idle_timer_;

   // Channel used to communicate read events.
   read_channel_type read_ch_;
   read_channel_type push_ch_;

   // Configuration parameters.
   config cfg_;

   // Called by the parser after each new chunk of resp3 data is
   // processed.
   adapter_type adapter_;

   // Buffer used by the read operations.
   std::string read_buffer_;

   // Info about the requests.
   struct req_info {
      write_channel_type channel;
      request_type* req = nullptr;
      bool sent = false;
   };

   // Request queue.
   std::queue<req_info> reqs_;

   // Last time we received data.
   time_point_type last_data_;

   // Used by the read_with_timeout_op.
   resp3::type type_;
   typename request_type::command_info_type cmd_info_;

   // See async_connect.
   boost::asio::ip::tcp::endpoint endpoint_;

   // See async_resolve.
   boost::asio::ip::tcp::resolver::results_type endpoints_;

   // write_op helper.
   std::size_t bytes_written_ = 0;
   request_type req_;
};

} // generic
} // aedis

#endif // AEDIS_GENERIC_CLIENT_HPP
