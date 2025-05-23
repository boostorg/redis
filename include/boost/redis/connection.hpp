/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONNECTION_HPP
#define BOOST_REDIS_CONNECTION_HPP

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/detail/connector.hpp>
#include <boost/redis/detail/health_checker.hpp>
#include <boost/redis/detail/helper.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/resolver.hpp>
#include <boost/redis/detail/resp3_handshaker.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/operation.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/usage.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_immediate_executor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/assert.hpp>
#include <boost/core/ignore_unused.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>

namespace boost::redis {
namespace detail {

template <class AsyncReadStream, class DynamicBuffer>
class append_some_op {
private:
   AsyncReadStream& stream_;
   DynamicBuffer buf_;
   std::size_t size_ = 0;
   std::size_t tmp_ = 0;
   asio::coroutine coro_{};

public:
   append_some_op(AsyncReadStream& stream, DynamicBuffer buf, std::size_t size)
   : stream_{stream}
   , buf_{std::move(buf)}
   , size_{size}
   { }

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t n = 0)
   {
      BOOST_ASIO_CORO_REENTER(coro_)
      {
         tmp_ = buf_.size();
         buf_.grow(size_);

         BOOST_ASIO_CORO_YIELD
         stream_.async_read_some(buf_.data(tmp_, size_), std::move(self));
         if (ec) {
            self.complete(ec, 0);
            return;
         }

         buf_.shrink(buf_.size() - tmp_ - n);
         self.complete({}, n);
      }
   }
};

template <class AsyncReadStream, class DynamicBuffer, class CompletionToken>
auto async_append_some(
   AsyncReadStream& stream,
   DynamicBuffer buffer,
   std::size_t size,
   CompletionToken&& token)
{
   return asio::async_compose<CompletionToken, void(system::error_code, std::size_t)>(
      append_some_op<AsyncReadStream, DynamicBuffer>{stream, buffer, size},
      token,
      stream);
}

template <class Executor>
using exec_notifier_type = asio::experimental::channel<
   Executor,
   void(system::error_code, std::size_t)>;

template <class Conn>
struct exec_op {
   using req_info_type = typename multiplexer::elem;
   using executor_type = typename Conn::executor_type;

   Conn* conn_ = nullptr;
   std::shared_ptr<exec_notifier_type<executor_type>> notifier_ = nullptr;
   std::shared_ptr<req_info_type> info_ = nullptr;
   asio::coroutine coro_{};

   template <class Self>
   void operator()(Self& self, system::error_code = {}, std::size_t = 0)
   {
      BOOST_ASIO_CORO_REENTER(coro_)
      {
         // Check whether the user wants to wait for the connection to
         // be stablished.
         if (info_->get_request().get_config().cancel_if_not_connected && !conn_->is_open()) {
            BOOST_ASIO_CORO_YIELD
            asio::dispatch(
               asio::get_associated_immediate_executor(self, self.get_io_executor()),
               std::move(self));
            return self.complete(error::not_connected, 0);
         }

         conn_->mpx_.add(info_);
         if (conn_->trigger_write()) {
            conn_->writer_timer_.cancel();
         }

EXEC_OP_WAIT:
         BOOST_ASIO_CORO_YIELD
         notifier_->async_receive(std::move(self));

         if (info_->get_error()) {
            self.complete(info_->get_error(), 0);
            return;
         }

         if (is_cancelled(self)) {
            if (!conn_->mpx_.remove(info_)) {
               using c_t = asio::cancellation_type;
               auto const c = self.get_cancellation_state().cancelled();
               if ((c & c_t::terminal) != c_t::none) {
                  // Cancellation requires closing the connection
                  // otherwise it stays in inconsistent state.
                  conn_->cancel(operation::run);
                  return self.complete(asio::error::operation_aborted, 0);
               } else {
                  // Can't implement other cancelation types, ignoring.
                  self.get_cancellation_state().clear();

                  // TODO: Find out a better way to ignore
                  // cancelation.
                  goto EXEC_OP_WAIT;
               }
            } else {
               // Cancelation honored.
               self.complete(asio::error::operation_aborted, 0);
               return;
            }
         }

         self.complete(info_->get_error(), info_->get_read_size());
      }
   }
};

template <class Conn, class Logger>
struct writer_op {
   Conn* conn_;
   Logger logger_;
   asio::coroutine coro{};

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t n = 0)
   {
      ignore_unused(n);

      BOOST_ASIO_CORO_REENTER(coro) for (;;)
      {
         while (conn_->mpx_.prepare_write() != 0) {
            if (conn_->use_ssl()) {
               BOOST_ASIO_CORO_YIELD
               asio::async_write(
                  conn_->next_layer(),
                  asio::buffer(conn_->mpx_.get_write_buffer()),
                  std::move(self));
            } else {
               BOOST_ASIO_CORO_YIELD
               asio::async_write(
                  conn_->next_layer().next_layer(),
                  asio::buffer(conn_->mpx_.get_write_buffer()),
                  std::move(self));
            }

            logger_.on_write(ec, conn_->mpx_.get_write_buffer());

            if (ec) {
               logger_.trace("writer_op (1)", ec);
               conn_->cancel(operation::run);
               self.complete(ec);
               return;
            }

            conn_->mpx_.commit_write();

            // A socket.close() may have been called while a
            // successful write might had already been queued, so we
            // have to check here before proceeding.
            if (!conn_->is_open()) {
               logger_.trace("writer_op (2): connection is closed.");
               self.complete({});
               return;
            }
         }

         BOOST_ASIO_CORO_YIELD
         conn_->writer_timer_.async_wait(std::move(self));
         if (!conn_->is_open()) {
            logger_.trace("writer_op (3): connection is closed.");
            // Notice this is not an error of the op, stoping was
            // requested from the outside, so we complete with
            // success.
            self.complete({});
            return;
         }
      }
   }
};

template <class Conn, class Logger>
struct reader_op {
   using dyn_buffer_type = asio::dynamic_string_buffer<
      char,
      std::char_traits<char>,
      std::allocator<char>>;

   // TODO: Move this to config so the user can fine tune?
   static constexpr std::size_t buffer_growth_hint = 4096;

   Conn* conn_;
   Logger logger_;
   std::pair<tribool, std::size_t> res_{std::make_pair(std::make_optional(false), 0)};
   asio::coroutine coro{};

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t n = 0)
   {
      ignore_unused(n);

      BOOST_ASIO_CORO_REENTER(coro) for (;;)
      {
         // Appends some data to the buffer if necessary.
         if (!res_.first.has_value() || conn_->mpx_.is_data_needed()) {
            if (conn_->use_ssl()) {
               BOOST_ASIO_CORO_YIELD
               async_append_some(
                  conn_->next_layer(),
                  dyn_buffer_type{conn_->mpx_.get_read_buffer(), conn_->cfg_.max_read_size},
                  conn_->mpx_.get_parser().get_suggested_buffer_growth(buffer_growth_hint),
                  std::move(self));
            } else {
               BOOST_ASIO_CORO_YIELD
               async_append_some(
                  conn_->next_layer().next_layer(),
                  dyn_buffer_type{conn_->mpx_.get_read_buffer(), conn_->cfg_.max_read_size},
                  conn_->mpx_.get_parser().get_suggested_buffer_growth(buffer_growth_hint),
                  std::move(self));
            }

            logger_.on_read(ec, n);

            // The connection is not viable after an error.
            if (ec) {
               logger_.trace("reader_op (1)", ec);
               conn_->cancel(operation::run);
               self.complete(ec);
               return;
            }

            // Somebody might have canceled implicitly or explicitly
            // while we were suspended and after queueing so we have to
            // check.
            if (!conn_->is_open()) {
               logger_.trace("reader_op (2): connection is closed.");
               self.complete(ec);
               return;
            }
         }

         res_ = conn_->mpx_.commit_read(ec);
         if (ec) {
            logger_.trace("reader_op (3)", ec);
            conn_->cancel(operation::run);
            self.complete(ec);
            return;
         }

         if (res_.first.has_value() && res_.first.value()) {
            if (!conn_->receive_channel_.try_send(ec, res_.second)) {
               BOOST_ASIO_CORO_YIELD
               conn_->receive_channel_.async_send(ec, res_.second, std::move(self));
            }

            if (ec) {
               logger_.trace("reader_op (4)", ec);
               conn_->cancel(operation::run);
               self.complete(ec);
               return;
            }

            if (!conn_->is_open()) {
               logger_.trace("reader_op (5): connection is closed.");
               self.complete(asio::error::operation_aborted);
               return;
            }
         }
      }
   }
};

template <class Conn, class Logger>
class run_op {
private:
   Conn* conn_ = nullptr;
   Logger logger_;
   asio::coroutine coro_{};

   using order_t = std::array<std::size_t, 5>;

public:
   run_op(Conn* conn, Logger l)
   : conn_{conn}
   , logger_{l}
   { }

   template <class Self>
   void operator()(
      Self& self,
      order_t order = {},
      system::error_code ec0 = {},
      system::error_code ec1 = {},
      system::error_code ec2 = {},
      system::error_code ec3 = {},
      system::error_code = {})
   {
      BOOST_ASIO_CORO_REENTER(coro_) for (;;)
      {
         BOOST_ASIO_CORO_YIELD
         conn_->resv_.async_resolve(asio::prepend(std::move(self), order_t{}));

         logger_.on_resolve(ec0, conn_->resv_.results());

         if (ec0) {
            self.complete(ec0);
            return;
         }

         BOOST_ASIO_CORO_YIELD
         conn_->ctor_.async_connect(
            conn_->next_layer().next_layer(),
            conn_->resv_.results(),
            asio::prepend(std::move(self), order_t{}));

         logger_.on_connect(ec0, conn_->ctor_.endpoint());

         if (ec0) {
            self.complete(ec0);
            return;
         }

         if (conn_->use_ssl()) {
            BOOST_ASIO_CORO_YIELD
            conn_->next_layer().async_handshake(
               asio::ssl::stream_base::client,
               asio::prepend(
                  asio::cancel_after(conn_->cfg_.ssl_handshake_timeout, std::move(self)),
                  order_t{}));

            logger_.on_ssl_handshake(ec0);

            if (ec0) {
               self.complete(ec0);
               return;
            }
         }

         conn_->mpx_.reset();

         // Note: Order is important here because the writer might
         // trigger an async_write before the async_hello thereby
         // causing an authentication problem.
         BOOST_ASIO_CORO_YIELD
         asio::experimental::make_parallel_group(
            [this](auto token) {
               return conn_->handshaker_.async_hello(*conn_, logger_, token);
            },
            [this](auto token) {
               return conn_->health_checker_.async_ping(*conn_, logger_, token);
            },
            [this](auto token) {
               return conn_->health_checker_.async_check_timeout(*conn_, logger_, token);
            },
            [this](auto token) {
               return conn_->reader(logger_, token);
            },
            [this](auto token) {
               return conn_->writer(logger_, token);
            })
            .async_wait(asio::experimental::wait_for_one_error(), std::move(self));

         if (order[0] == 0 && !!ec0) {
            self.complete(ec0);
            return;
         }

         if (order[0] == 2 && ec2 == error::pong_timeout) {
            self.complete(ec1);
            return;
         }

         // The receive operation must be cancelled because channel
         // subscription does not survive a reconnection but requires
         // re-subscription.
         conn_->cancel(operation::receive);

         if (!conn_->will_reconnect()) {
            conn_->cancel(operation::reconnection);
            self.complete(ec3);
            return;
         }

         conn_->reconnect_timer_.expires_after(conn_->cfg_.reconnect_wait_interval);

         BOOST_ASIO_CORO_YIELD
         conn_->reconnect_timer_.async_wait(asio::prepend(std::move(self), order_t{}));
         if (ec0) {
            self.complete(ec0);
            return;
         }

         if (!conn_->will_reconnect()) {
            self.complete(asio::error::operation_aborted);
            return;
         }

         conn_->reset_stream();
      }
   }
};

}  // namespace detail

/** @brief A SSL connection to the Redis server.
 *  @ingroup high-level-api
 *
 *  This class keeps a healthy connection to the Redis instance where
 *  commands can be sent at any time. For more details, please see the
 *  documentation of each individual function.
 *
 *  @tparam Socket The socket type e.g. asio::ip::tcp::socket.
 *
 */
template <class Executor>
class basic_connection {
public:
   using this_type = basic_connection<Executor>;

   /// Type of the next layer
   using next_layer_type = asio::ssl::stream<asio::basic_stream_socket<asio::ip::tcp, Executor>>;

   /// Executor type
   using executor_type = Executor;

   /// Returns the associated executor.
   executor_type get_executor() noexcept { return writer_timer_.get_executor(); }

   /// Rebinds the socket type to another executor.
   template <class Executor1>
   struct rebind_executor {
      /// The connection type when rebound to the specified executor.
      using other = basic_connection<Executor1>;
   };

   /** @brief Constructor
   *
   *  @param ex Executor on which connection operation will run.
   *  @param ctx SSL context.
   */
   explicit basic_connection(
      executor_type ex,
      asio::ssl::context ctx = asio::ssl::context{asio::ssl::context::tlsv12_client})
   : ctx_{std::move(ctx)}
   , stream_{std::make_unique<next_layer_type>(ex, ctx_)}
   , writer_timer_{ex}
   , reconnect_timer_{ex}
   , receive_channel_{ex, 256}
   , resv_{ex}
   , health_checker_{ex}
   {
      set_receive_response(ignore);
      writer_timer_.expires_at((std::chrono::steady_clock::time_point::max)());
   }

   /// Constructs from a context.
   explicit basic_connection(
      asio::io_context& ioc,
      asio::ssl::context ctx = asio::ssl::context{asio::ssl::context::tlsv12_client})
   : basic_connection(ioc.get_executor(), std::move(ctx))
   { }

   /** @brief Starts underlying connection operations.
    *
    *  This member function provides the following functionality
    *
    *  1. Resolve the address passed on `boost::redis::config::addr`.
    *  2. Connect to one of the results obtained in the resolve operation.
    *  3. Send a [HELLO](https://redis.io/commands/hello/) command where each of its parameters are read from `cfg`.
    *  4. Start a health-check operation where ping commands are sent
    *     at intervals specified in
    *     `boost::redis::config::health_check_interval`.  The message passed to
    *     `PING` will be `boost::redis::config::health_check_id`.  Passing a
    *     timeout with value zero will disable health-checks.  If the Redis
    *     server does not respond to a health-check within two times the value
    *     specified here, it will be considered unresponsive and the connection
    *     will be closed and a new connection will be stablished.
    *  5. Starts read and write operations with the Redis
    *  server. More specifically it will trigger the write of all
    *  requests i.e. calls to `async_exec` that happened prior to this
    *  call.
    *
    *  When a connection is lost for any reason, a new one is
    *  stablished automatically. To disable reconnection call
    *  `boost::redis::connection::cancel(operation::reconnection)`.
    *
    *  @param cfg Configuration paramters.
    *  @param l Logger object. The interface expected is specified in the class `boost::redis::logger`.
    *  @param token Completion token.
    *
    *  The completion token must have the following signature
    *
    *  @code
    *  void f(system::error_code);
    *  @endcode
    *
    *  For example on how to call this function refer to
    *  cpp20_intro.cpp or any other example.
    */
   template <
      class Logger = logger,
      class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_run(config const& cfg = {}, Logger l = Logger{}, CompletionToken&& token = {})
   {
      cfg_ = cfg;
      resv_.set_config(cfg);
      ctor_.set_config(cfg);
      health_checker_.set_config(cfg);
      handshaker_.set_config(cfg);
      l.set_prefix(cfg.log_prefix);

      return asio::async_compose<CompletionToken, void(system::error_code)>(
         detail::run_op<this_type, Logger>{this, l},
         token,
         writer_timer_);
   }

   /** @brief Receives server side pushes asynchronously.
    *
    *  When pushes arrive and there is no `async_receive` operation in
    *  progress, pushed data, requests, and responses will be paused
    *  until `async_receive` is called again.  Apps will usually want
    *  to call `async_receive` in a loop. 
    *
    *  To cancel an ongoing receive operation apps should call
    *  `connection::cancel(operation::receive)`.
    *
    *  @param token Completion token.
    *
    *  For an example see cpp20_subscriber.cpp. The completion token must
    *  have the following signature
    *
    *  @code
    *  void f(system::error_code, std::size_t);
    *  @endcode
    *
    *  Where the second parameter is the size of the push received in
    *  bytes.
    */
   template <class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_receive(CompletionToken&& token = {})
   {
      return receive_channel_.async_receive(std::forward<CompletionToken>(token));
   }

   /** @brief Receives server pushes synchronously without blocking.
    *
    *  Receives a server push synchronously by calling `try_receive` on
    *  the underlying channel. If the operation fails because
    *  `try_receive` returns `false`, `ec` will be set to
    *  `boost::redis::error::sync_receive_push_failed`.
    *
    *  @param ec Contains the error if any occurred.
    *
    *  @returns The number of bytes read from the socket.
    */
   std::size_t receive(system::error_code& ec)
   {
      std::size_t size = 0;

      auto f = [&](system::error_code const& ec2, std::size_t n) {
         ec = ec2;
         size = n;
      };

      auto const res = receive_channel_.try_receive(f);
      if (ec)
         return 0;

      if (!res)
         ec = error::sync_receive_push_failed;

      return size;
   }

   /** @brief Executes commands on the Redis server asynchronously.
    *
    *  This function sends a request to the Redis server and waits for
    *  the responses to each individual command in the request. If the
    *  request contains only commands that don't expect a response,
    *  the completion occurs after it has been written to the
    *  underlying stream.  Multiple concurrent calls to this function
    *  will be automatically queued by the implementation.
    *
    *  @param req Request.
    *  @param resp Response.
    *  @param token Completion token.
    *
    *  For an example see cpp20_echo_server.cpp. The completion token must
    *  have the following signature
    *
    *  @code
    *  void f(system::error_code, std::size_t);
    *  @endcode
    *
    *  Where the second parameter is the size of the response received
    *  in bytes.
    */
   template <
      class Response = ignore_t,
      class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_exec(request const& req, Response& resp = ignore, CompletionToken&& token = {})
   {
      return this->async_exec(req, any_adapter(resp), std::forward<CompletionToken>(token));
   }

   /** @copydoc async_exec
    * 
    * @details This function uses the type-erased @ref any_adapter class, which
    * encapsulates a reference to a response object.
    */
   template <class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_exec(request const& req, any_adapter adapter, CompletionToken&& token = {})
   {
      auto& adapter_impl = adapter.impl_;
      BOOST_ASSERT_MSG(
         req.get_expected_responses() <= adapter_impl.supported_response_size,
         "Request and response have incompatible sizes.");

      auto notifier = std::make_shared<detail::exec_notifier_type<executor_type>>(
         get_executor(),
         1);
      auto info = detail::make_elem(req, std::move(adapter_impl.adapt_fn));

      info->set_done_callback([notifier]() {
         notifier->try_send(std::error_code{}, 0);
      });

      return asio::async_compose<CompletionToken, void(system::error_code, std::size_t)>(
         detail::exec_op<this_type>{this, notifier, info},
         token,
         writer_timer_);
   }

   /** @brief Cancel operations.
    *
    *  @li `operation::exec`: Cancels operations started with
    *  `async_exec`. Affects only requests that haven't been written
    *  yet.
    *  @li operation::run: Cancels the `async_run` operation.
    *  @li operation::receive: Cancels any ongoing calls to `async_receive`.
    *  @li operation::all: Cancels all operations listed above.
    *
    *  @param op: The operation to be cancelled.
    */
   void cancel(operation op = operation::all)
   {
      switch (op) {
         case operation::resolve: resv_.cancel(); break;
         case operation::exec:    mpx_.cancel_waiting(); break;
         case operation::reconnection:
            cfg_.reconnect_wait_interval = std::chrono::seconds::zero();
            break;
         case operation::run:          cancel_run(); break;
         case operation::receive:      receive_channel_.cancel(); break;
         case operation::health_check: health_checker_.cancel(); break;
         case operation::all:
            resv_.cancel();
            cfg_.reconnect_wait_interval = std::chrono::seconds::zero();
            health_checker_.cancel();
            cancel_run();               // run
            receive_channel_.cancel();  // receive
            mpx_.cancel_waiting();      // exec
            break;
         default: /* ignore */;
      }
   }

   auto run_is_canceled() const noexcept { return mpx_.get_cancel_run_state(); }

   /// Returns true if the connection was canceled.
   bool will_reconnect() const noexcept
   {
      return cfg_.reconnect_wait_interval != std::chrono::seconds::zero();
   }

   /// Returns the ssl context.
   auto const& get_ssl_context() const noexcept { return ctx_; }

   /// Resets the underlying stream.
   void reset_stream()
   {
      stream_ = std::make_unique<next_layer_type>(writer_timer_.get_executor(), ctx_);
   }

   /// Returns a reference to the next layer.
   auto& next_layer() noexcept { return *stream_; }

   /// Returns a const reference to the next layer.
   auto const& next_layer() const noexcept { return *stream_; }

   /// Sets the response object of `async_receive` operations.
   template <class Response>
   void set_receive_response(Response& response)
   {
      mpx_.set_receive_response(response);
   }

   /// Returns connection usage information.
   usage get_usage() const noexcept { return mpx_.get_usage(); }

private:
   using clock_type = std::chrono::steady_clock;
   using clock_traits_type = asio::wait_traits<clock_type>;
   using timer_type = asio::basic_waitable_timer<clock_type, clock_traits_type, executor_type>;

   using receive_channel_type = asio::experimental::channel<
      executor_type,
      void(system::error_code, std::size_t)>;
   using resolver_type = detail::resolver<Executor>;
   using health_checker_type = detail::health_checker<Executor>;
   using resp3_handshaker_type = detail::resp3_handshaker<executor_type>;

   auto use_ssl() const noexcept { return cfg_.use_ssl; }

   void cancel_run()
   {
      close();
      writer_timer_.cancel();
      receive_channel_.cancel();
      mpx_.cancel_on_conn_lost();
   }

   template <class, class> friend struct detail::reader_op;
   template <class, class> friend struct detail::writer_op;
   template <class> friend struct detail::exec_op;
   template <class, class> friend class detail::run_op;

   template <class CompletionToken, class Logger>
   auto reader(Logger l, CompletionToken&& token)
   {
      return asio::async_compose<CompletionToken, void(system::error_code)>(
         detail::reader_op<this_type, Logger>{this, l},
         std::forward<CompletionToken>(token),
         writer_timer_);
   }

   template <class CompletionToken, class Logger>
   auto writer(Logger l, CompletionToken&& token)
   {
      return asio::async_compose<CompletionToken, void(system::error_code)>(
         detail::writer_op<this_type, Logger>{this, l},
         std::forward<CompletionToken>(token),
         writer_timer_);
   }

   void close()
   {
      if (stream_->next_layer().is_open()) {
         system::error_code ec;
         stream_->next_layer().close(ec);
      }
   }

   auto is_open() const noexcept { return stream_->next_layer().is_open(); }
   auto& lowest_layer() noexcept { return stream_->lowest_layer(); }

   [[nodiscard]] bool trigger_write() const noexcept { return is_open() && !mpx_.is_writing(); }

   asio::ssl::context ctx_;
   std::unique_ptr<next_layer_type> stream_;

   // Notice we use a timer to simulate a condition-variable. It is
   // also more suitable than a channel and the notify operation does
   // not suspend.
   timer_type writer_timer_;
   timer_type reconnect_timer_;  // to wait the reconnection period
   receive_channel_type receive_channel_;
   resolver_type resv_;
   detail::connector ctor_;
   health_checker_type health_checker_;
   resp3_handshaker_type handshaker_;

   config cfg_;
   detail::multiplexer mpx_;
};

/** \brief A basic_connection that type erases the executor.
 *  \ingroup high-level-api
 *
 *  This connection type uses the asio::any_io_executor and
 *  asio::any_completion_token to reduce compilation times.
 *
 *  For documentation of each member function see
 *  `boost::redis::basic_connection`.
 */
class connection {
public:
   /// Executor type.
   using executor_type = asio::any_io_executor;

   /// Contructs from an executor.
   explicit connection(
      executor_type ex,
      asio::ssl::context ctx = asio::ssl::context{asio::ssl::context::tlsv12_client});

   /// Contructs from a context.
   explicit connection(
      asio::io_context& ioc,
      asio::ssl::context ctx = asio::ssl::context{asio::ssl::context::tlsv12_client});

   /// Returns the underlying executor.
   executor_type get_executor() noexcept { return impl_.get_executor(); }

   /// Calls `boost::redis::basic_connection::async_run`.
   template <class CompletionToken = asio::deferred_t>
   auto async_run(config const& cfg, logger l, CompletionToken&& token = {})
   {
      return asio::async_initiate<CompletionToken, void(boost::system::error_code)>(
         [](auto handler, connection* self, config const* cfg, logger l) {
            self->async_run_impl(*cfg, l, std::move(handler));
         },
         token,
         this,
         &cfg,
         l);
   }

   /// Calls `boost::redis::basic_connection::async_receive`.
   template <class CompletionToken = asio::deferred_t>
   auto async_receive(CompletionToken&& token = {})
   {
      return impl_.async_receive(std::forward<CompletionToken>(token));
   }

   /// Calls `boost::redis::basic_connection::receive`.
   std::size_t receive(system::error_code& ec) { return impl_.receive(ec); }

   /// Calls `boost::redis::basic_connection::async_exec`.
   template <class Response, class CompletionToken = asio::deferred_t>
   auto async_exec(request const& req, Response& resp, CompletionToken&& token = {})
   {
      return async_exec(req, any_adapter(resp), std::forward<CompletionToken>(token));
   }

   /// Calls `boost::redis::basic_connection::async_exec`.
   template <class CompletionToken = asio::deferred_t>
   auto async_exec(request const& req, any_adapter adapter, CompletionToken&& token = {})
   {
      return asio::async_initiate<CompletionToken, void(boost::system::error_code, std::size_t)>(
         [](auto handler, connection* self, request const* req, any_adapter&& adapter) {
            self->async_exec_impl(*req, std::move(adapter), std::move(handler));
         },
         token,
         this,
         &req,
         std::move(adapter));
   }

   /// Calls `boost::redis::basic_connection::cancel`.
   void cancel(operation op = operation::all);

   /// Calls `boost::redis::basic_connection::will_reconnect`.
   bool will_reconnect() const noexcept { return impl_.will_reconnect(); }

   /// Calls `boost::redis::basic_connection::next_layer`.
   auto& next_layer() noexcept { return impl_.next_layer(); }

   /// Calls `boost::redis::basic_connection::next_layer`.
   auto const& next_layer() const noexcept { return impl_.next_layer(); }

   /// Calls `boost::redis::basic_connection::reset_stream`.
   void reset_stream() { impl_.reset_stream(); }

   /// Sets the response object of `async_receive` operations.
   template <class Response>
   void set_receive_response(Response& response)
   {
      impl_.set_receive_response(response);
   }

   /// Returns connection usage information.
   usage get_usage() const noexcept { return impl_.get_usage(); }

   /// Returns the ssl context.
   auto const& get_ssl_context() const noexcept { return impl_.get_ssl_context(); }

private:
   void async_run_impl(
      config const& cfg,
      logger l,
      asio::any_completion_handler<void(boost::system::error_code)> token);

   void async_exec_impl(
      request const& req,
      any_adapter&& adapter,
      asio::any_completion_handler<void(boost::system::error_code, std::size_t)> token);

   basic_connection<executor_type> impl_;
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_CONNECTION_HPP
