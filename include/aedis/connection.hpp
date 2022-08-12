/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_CONNECTION_HPP
#define AEDIS_CONNECTION_HPP

#include <vector>
#include <queue>
#include <limits>
#include <chrono>
#include <memory>
#include <type_traits>

#include <boost/assert.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/experimental/channel.hpp>

#include <aedis/adapt.hpp>
#include <aedis/resp3/request.hpp>
#include <aedis/detail/connection_ops.hpp>

namespace aedis {

/** @brief A high level Redis connection class.
 *  @ingroup any
 *
 *  This class keeps a healthy connection to the Redis instance where
 *  commands can be sent at any time. For more details, please see the
 *  documentation of each individual function.
 *
 */
template <class AsyncReadWriteStream = boost::asio::ip::tcp::socket>
class connection {
public:
   /// Executor type.
   using executor_type = typename AsyncReadWriteStream::executor_type;

   /// Type of the next layer
   using next_layer_type = AsyncReadWriteStream;

   using default_completion_token_type = boost::asio::default_completion_token_t<executor_type>;
   using channel_type = boost::asio::experimental::channel<executor_type, void(boost::system::error_code, std::size_t)>;
   using clock_type = std::chrono::steady_clock;
   using clock_traits_type = boost::asio::wait_traits<clock_type>;
   using timer_type = boost::asio::basic_waitable_timer<clock_type, clock_traits_type, executor_type>;
   using resolver_type = boost::asio::ip::basic_resolver<boost::asio::ip::tcp, executor_type>;

   /** @brief Connection configuration parameters.
    */
   struct config {
      /// The Redis server address.
      std::string host = "127.0.0.1";

      /// The Redis server port.
      std::string port = "6379";

      /// Username if authentication is required.
      std::string username;

      /// Password if authentication is required.
      std::string password;

      /// Timeout of the resolve operation.
      std::chrono::milliseconds resolve_timeout = std::chrono::seconds{10};

      /// Timeout of the connect operation.
      std::chrono::milliseconds connect_timeout = std::chrono::seconds{10};

      /// Time interval ping operations.
      std::chrono::milliseconds ping_interval = std::chrono::seconds{1};

      /// Time waited before trying a reconnection (see enable reconnect).
      std::chrono::milliseconds reconnect_interval = std::chrono::seconds{1};

      /// The maximum size allowed on read operations.
      std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)();

      /// Whether to coalesce requests (see [pipelines](https://redis.io/topics/pipelining)).
      bool coalesce_requests = true;

      /// Enable events
      bool enable_events = false;

      /// Enable automatic reconnection (see also reconnect_interval).
      bool enable_reconnect = false;
   };

   /// Events communicated through \c async_receive_event.
   enum class event {
      /// The address has been successfully resolved.
      resolve,
      /// Connected to the Redis server.
      connect,
      /// Success sending AUTH and HELLO.
      hello,
      /// A push event has been received.
      push,
      /// Used internally.
      invalid
   };

   /** @brief Async operations that can be cancelled.
    *  
    *  See the \c cancel member function for more information.
    */
   enum class operation {
      /// Operations started with \c async_exec.
      exec,
      /// Operation started with \c async_run.
      run,
      /// Operation started with async_receive_event.
      receive_event
   };

   /** \brief Construct a connection from an executor.
    *
    *  \param ex The executor.
    *  \param cfg Configuration parameters.
    */
   connection(executor_type ex, config cfg = config{})
   : resv_{ex}
   , ping_timer_{ex}
   , check_idle_timer_{ex}
   , writer_timer_{ex}
   , read_timer_{ex}
   , push_channel_{ex}
   , cfg_{cfg}
   , last_data_{std::chrono::time_point<std::chrono::steady_clock>::min()}
   {
      writer_timer_.expires_at(std::chrono::steady_clock::time_point::max());
      read_timer_.expires_at(std::chrono::steady_clock::time_point::max());
   }

   /** \brief Construct a connection from an io_context.
    *
    *  \param ioc The executor.
    *  \param cfg Configuration parameters.
    */
   connection(boost::asio::io_context& ioc, config cfg = config{})
   : connection(ioc.get_executor(), cfg)
   { }

   /// Returns the executor.
   auto get_executor() {return resv_.get_executor();}

   /** @brief Starts communication with the Redis server asynchronously.
    *
    *  This function performs the following steps
    *
    *  \li Resolves the Redis host as of \c async_resolve with the
    *  timeout passed in connection::config::resolve_timeout.
    *
    *  \li Connects to one of the endpoints returned by the resolve
    *  operation with the timeout passed in connection::config::connect_timeout.
    *
    *  \li Starts the idle check operation with the timeout of twice
    *  the value of connection::config::ping_interval. If no data is
    *  received during that time interval \c async_run completes with
    *  error::idle_timeout.
    *
    *  \li Starts the healthy check operation that sends command::ping
    *  to Redis with a frequency equal to
    *  connection::config::ping_interval.
    *
    *  \li Starts reading from the socket and delivering events to the
    *  request started with \c async_exec and \c async_receive_event.
    *
    * For an example see echo_server.cpp.
    *
    *  \param token Completion token.
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
         >(detail::run_op<connection>{this}, token, resv_);
   }

   /** @brief Executes a command on the redis server asynchronously.
    *
    *  \param req Request object.
    *  \param adapter Response adapter.
    *  \param token Asio completion token.
    *
    *  For an example see echo_server.cpp. The completion token must
    *  have the following signature
    *
    *  @code
    *  void f(boost::system::error_code, std::size_t);
    *  @endcode
    *
    *  Where the second parameter is the size of the response in
    *  bytes.
    */
   template <
      class Adapter = detail::response_traits<void>::adapter_type,
      class CompletionToken = default_completion_token_type>
   auto async_exec(
      resp3::request const& req,
      Adapter adapter = adapt(),
      CompletionToken token = CompletionToken{})
   {
      BOOST_ASSERT_MSG(req.size() <= adapter.supported_response_size(), "Request and adapter have incompatible sizes.");

      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t)
         >(detail::exec_op<connection, Adapter>{this, &req, adapter}, token, resv_);
   }

   /** @brief Connects and executes a request asynchronously.
    *
    *  Combines \c async_run and the other \c async_exec overload in a
    *  single function. This function is useful for users that want to
    *  send a single request to the server and close it.
    *
    *  \param req Request object.
    *  \param adapter Response adapter.
    *  \param token Asio completion token.
    *
    *  For an example see intro.cpp. The completion token must have the following signature
    *
    *  @code
    *  void f(boost::system::error_code, std::size_t);
    *  @endcode
    *
    *  Where the second parameter is the size of the response in bytes.
    */
   template <
      class Adapter = detail::response_traits<void>::adapter_type,
      class CompletionToken = default_completion_token_type>
   auto async_run(
      resp3::request const& req,
      Adapter adapter = adapt(),
      CompletionToken token = CompletionToken{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t)
         >(detail::runexec_op<connection, Adapter>
            {this, &req, adapter}, token, resv_);
   }

   /** @brief Receives unsolicited events asynchronously.
    *
    *  Users that expect unsolicited events should call this function
    *  in a loop. If an unsolicited events comes in and there is no
    *  reader, the connection will hang and eventually timeout.
    *
    *  \param adapter The response adapter.
    *  \param token The Asio completion token.
    *
    *  For an example see subscriber.cpp. The completion token must
    *  have the following signature
    *
    *  @code
    *  void f(boost::system::error_code, event);
    *  @endcode
    *
    *  Where the second parameter is the size of the response that has
    *  just been read.
    */
   template <
      class Adapter = detail::response_traits<void>::adapter_type,
      class CompletionToken = default_completion_token_type>
   auto async_receive_event(
      Adapter adapter = adapt(),
      CompletionToken token = CompletionToken{})
   {
      auto f =
         [adapter]
         (resp3::node<boost::string_view> const& node, boost::system::error_code& ec) mutable
      {
         adapter(std::size_t(-1), node, ec);
      };

      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, event)
         >(detail::receive_op<connection, decltype(f)>{this, f}, token, resv_);
   }

   /** @brief Cancel operations.
    *
    * @li operation::exec: Cancels all operations started with \c async_exec.
    * @li operation::run: Cancels @c async_run. The prefered way to
    *     close a connection is to set config::enable_reconnect to
    *     false and send a \c quit command. Otherwise an unresponsive Redis server
    *     will cause the idle-checks to kick in and lead to \c
    *     async_run returning with idle_timeout. Calling \c
    *     cancel(operation::run) directly should be seen as the last
    *     option.
    * @li operation::receive_event: Cancels @c async_receive_event.
    *
    * @param op: The operation to be cancelled.
    * @returns The number of operations that have been canceled.
    */
   std::size_t cancel(operation op)
   {
      switch (op) {
         case operation::exec:
         {
            for (auto& e: reqs_) {
               e->stop = true;
               e->timer.cancel_one();
            }

            auto const ret = reqs_.size();
            reqs_ = {};
            return ret;
         }
         case operation::run:
         {
            if (socket_)
               socket_->close();

            read_timer_.cancel();
            check_idle_timer_.cancel();
            writer_timer_.cancel();
            ping_timer_.cancel();

            // Cancel own pings if there are any waiting.
            auto point = std::stable_partition(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
               return !ptr->req->close_on_run_completion;
            });

            std::for_each(point, std::end(reqs_), [](auto const& ptr) {
               ptr->stop = true;
               ptr->timer.cancel();
            });

            reqs_.erase(point, std::end(reqs_));
            return 1U;
         }
         case operation::receive_event:
         {
            push_channel_.cancel();
            return 1U;
         }
      }
   }

   /// Get the config object.
   config& get_config() noexcept { return cfg_;}

   /// Gets the config object.
   config const& get_config() const noexcept { return cfg_;}

private:
   struct req_info {
      req_info(executor_type ex) : timer{ex} {}
      timer_type timer;
      resp3::request const* req = nullptr;
      std::size_t cmds = 0;
      bool stop = false;
      bool written = false;
   };

   using time_point_type = std::chrono::time_point<std::chrono::steady_clock>;
   using reqs_type = std::deque<std::shared_ptr<req_info>>;

   template <class T, class U> friend struct detail::receive_op;
   template <class T> friend struct detail::reader_op;
   template <class T> friend struct detail::writer_op;
   template <class T> friend struct detail::ping_op;
   template <class T> friend struct detail::run_op;
   template <class T> friend struct detail::run_one_op;
   template <class T, class U> friend struct detail::exec_op;
   template <class T, class U> friend struct detail::exec_read_op;
   template <class T, class U> friend struct detail::runexec_op;
   template <class T> friend struct detail::connect_with_timeout_op;
   template <class T> friend struct detail::resolve_with_timeout_op;
   template <class T> friend struct detail::check_idle_op;
   template <class T> friend struct detail::start_op;
   template <class T> friend struct detail::send_receive_op;

   template <class CompletionToken = default_completion_token_type>
   auto async_run_one(CompletionToken token = CompletionToken{})
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::run_one_op<connection>{this}, token, resv_);
   }

   void cancel_push_requests()
   {
      auto point = std::stable_partition(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
         return !(ptr->written && ptr->req->size() == 0);
      });

      std::for_each(point, std::end(reqs_), [](auto const& ptr) {
         ptr->timer.cancel();
      });

      reqs_.erase(point, std::end(reqs_));
   }

   void add_request_info(std::shared_ptr<req_info> const& info)
   {
      reqs_.push_back(info);
      if (socket_ != nullptr && socket_->is_open() && cmds_ == 0 && write_buffer_.empty())
         writer_timer_.cancel();
   }

   auto make_dynamic_buffer()
      { return boost::asio::dynamic_buffer(read_buffer_, cfg_.max_read_size); }

   template <class CompletionToken>
   auto async_resolve_with_timeout(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::resolve_with_timeout_op<connection>{this},
            token, resv_);
   }

   template <class CompletionToken>
   auto async_connect_with_timeout(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::connect_with_timeout_op<connection>{this}, token, resv_);
   }

   template <class CompletionToken>
   auto reader(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::reader_op<connection>{this}, token, resv_.get_executor());
   }

   template <class CompletionToken>
   auto writer(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::writer_op<connection>{this}, token, resv_.get_executor());
   }

   template <class CompletionToken>
   auto
   async_start(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::start_op<connection>{this}, token, resv_);
   }

   template <class CompletionToken>
   auto async_ping(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::ping_op<connection>{this}, token, resv_);
   }

   template <class CompletionToken>
   auto async_check_idle(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::check_idle_op<connection>{this}, token, check_idle_timer_);
   }

   template <class Adapter, class CompletionToken>
   auto async_exec_read(Adapter adapter, std::size_t cmds, CompletionToken token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code, std::size_t)
         >(detail::exec_read_op<connection, Adapter>{this, adapter, cmds}, token, resv_);
   }

   void coalesce_requests()
   {
      // Coaleces all requests: Copies the request to the variables
      // that won't be touched while async_write is suspended.
      BOOST_ASSERT(write_buffer_.empty());
      BOOST_ASSERT(!reqs_.empty());

      auto const size = cfg_.coalesce_requests ? reqs_.size() : 1;
      for (auto i = 0UL; i < size; ++i) {
         write_buffer_ += reqs_.at(i)->req->payload();
         cmds_ += reqs_.at(i)->req->size();
         reqs_.at(i)->written = true;
      }
   }

   // IO objects
   resolver_type resv_;
   std::shared_ptr<AsyncReadWriteStream> socket_;
   timer_type ping_timer_;
   timer_type check_idle_timer_;
   timer_type writer_timer_;
   timer_type read_timer_;
   channel_type push_channel_;

   config cfg_;
   std::string read_buffer_;
   std::string write_buffer_;
   std::size_t cmds_ = 0;
   reqs_type reqs_;
   event last_event_ = event::invalid;

   // Last time we received data.
   time_point_type last_data_;

   // The result of async_resolve.
   boost::asio::ip::tcp::resolver::results_type endpoints_;

   resp3::request req_;
};

/// Converts a connection event to a string.
template <class T>
char const* to_string(typename connection<T>::event e)
{
   using event_type = typename connection<T>::event;
   switch (e) {
      case event_type::resolve: return "resolve";
      case event_type::connect: return "connect";
      case event_type::hello: return "hello";
      case event_type::push: return "push";
      case event_type::invalid: return "invalid";
      default: BOOST_ASSERT_MSG(false, "to_string: unhandled event.");
   }
}

/// Writes a connection event to the stream.
template <class T>
std::ostream& operator<<(std::ostream& os, typename connection<T>::event e)
{
   os << to_string(e);
   return os;
}

} // aedis

#endif // AEDIS_CONNECTION_HPP
