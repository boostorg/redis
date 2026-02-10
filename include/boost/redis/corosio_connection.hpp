/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_COROSIO_CONNECTION_HPP
#define BOOST_REDIS_COROSIO_CONNECTION_HPP

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/exec_fsm.hpp>
#include <boost/redis/detail/exec_one_fsm.hpp>
#include <boost/redis/detail/flow_controller.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/reader_fsm.hpp>
#include <boost/redis/detail/receive_fsm.hpp>
#include <boost/redis/detail/redis_stream.hpp>
#include <boost/redis/detail/run_fsm.hpp>
#include <boost/redis/detail/sentinel_resolve_fsm.hpp>
#include <boost/redis/detail/writer_fsm.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/operation.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>
#include <boost/redis/usage.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/assert.hpp>
#include <boost/capy/buffers/make_buffer.hpp>
#include <boost/capy/ex/async_event.hpp>
#include <boost/capy/ex/execution_context.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_result.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/write.hpp>
#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/corosio/timer.hpp>
#include <boost/system/detail/error_code.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>

namespace boost::redis {
namespace detail {

// Given a timeout value, compute the expiry time. A zero timeout is considered to mean "no timeout"
inline std::chrono::steady_clock::time_point compute_expiry(
   std::chrono::steady_clock::duration timeout)
{
   return timeout.count() == 0 ? (std::chrono::steady_clock::time_point::max)()
                               : std::chrono::steady_clock::now() + timeout;
}

inline asio::cancellation_type_t token_to_cancel(std::stop_token tok)
{
   return tok.stop_requested() ? asio::cancellation_type_t::partial
                               : asio::cancellation_type_t::none;
}

// TODO: actually implement
class corosio_redis_stream {
   //    asio::ssl::context ssl_ctx_;
   //    asio::ip::basic_resolver<asio::ip::tcp, Executor> resolv_;
   //    asio::ssl::stream<asio::basic_stream_socket<asio::ip::tcp, Executor>> stream_;
   // #ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
   //    asio::basic_stream_socket<asio::local::stream_protocol, Executor> unix_socket_;
   // #endif
   //    typename asio::steady_timer::template rebind_executor<Executor>::other timer_;
   redis_stream_state st_;

public:
   // I/O
   capy::io_task<> connect(const connect_params& params, buffered_logger& l);

   template <class ConstBufferSequence>
   capy::io_task<std::size_t> write_some(const ConstBufferSequence& buffers);

   template <class MutableBufferSequence>
   capy::io_task<std::size_t> read_some(const MutableBufferSequence& buffers);
};

struct corosio_connection_impl {
   // using receive_channel_type = asio::experimental::channel<
   //    Executor,
   //    void(system::error_code, std::size_t)>;

   corosio_redis_stream stream_;
   corosio::timer writer_timer_;     // timer used for write timeouts
   capy::async_event writer_event_;  // set when there is new data to write
   corosio::timer reader_timer_;     // timer used for read timeouts
   corosio::timer reconnect_timer_;  // to wait the reconnection period
   corosio::timer ping_timer_;       // to wait between pings
   // receive_channel_type receive_channel_;
   asio::cancellation_signal run_signal_;
   connection_state st_;
   flow_controller controller_;

   corosio_connection_impl(capy::execution_context& ex, logger&& lgr)
   : stream_{ex}
   , writer_timer_{ex}
   , writer_event_{ex}
   , reader_timer_{ex}
   , reconnect_timer_{ex}
   , ping_timer_{ex}  // , receive_channel_{ex, 256}
   , st_{{std::move(lgr)}}
   {
      set_receive_adapter(any_adapter{ignore});
      writer_event_.expires_at((std::chrono::steady_clock::time_point::max)());
   }

   void cancel(operation op)
   {
      switch (op) {
         case operation::exec:    st_.mpx.cancel_waiting(); break;
         case operation::receive: cancel_receive_v2(); break;
         case operation::reconnection:
            st_.cfg.reconnect_wait_interval = std::chrono::seconds::zero();
            break;
         case operation::run:
         case operation::resolve:
         case operation::connect:
         case operation::ssl_handshake:
         case operation::health_check:  cancel_run(); break;
         case operation::all:
            st_.mpx.cancel_waiting();                                        // exec
            cancel_receive_v2();                                             // receive
            st_.cfg.reconnect_wait_interval = std::chrono::seconds::zero();  // reconnect
            cancel_run();                                                    // run
            break;
         default: /* ignore */;
      }
   }

   void cancel_receive_v1();

   void cancel_receive_v2()
   {
      st_.receive2_cancelled = true;
      cancel_receive_v1();
   }

   // void cancel_run()
   // {
   //    // Individual operations should see a terminal cancellation, regardless
   //    // of what we got requested. We take enough actions to ensure that this
   //    // doesn't prevent the object from being re-used (e.g. we reset the TLS stream).
   //    run_signal_.emit(asio::cancellation_type_t::terminal);

   //    // Name resolution doesn't support per-operation cancellation
   //    stream_.cancel_resolve();

   //    // Receive is technically not part of run, but we also cancel it for
   //    // backwards compatibility. Note that this intentionally affects v1 receive, only.
   //    cancel_receive_v1();
   // }

   capy::io_task<> async_exec(request const& req, any_adapter adapter)
   {
      // Setup
      capy::async_event request_done;
      auto elem = make_elem(req, std::move(adapter));
      elem->set_done_callback([&request_done]() {
         request_done.set();
      });
      exec_fsm fsm{elem};

      // Invoke the FSM
      while (true) {
         // Invoke the state machine
         auto act = fsm.resume(true, st_, token_to_cancel(co_await capy::this_coro::stop_token));

         // Do what the FSM said
         switch (act.type()) {
            case exec_action_type::setup_cancellation: break;  // ignored, not required by capy
            case exec_action_type::immediate:          break;  // ignored, not required by capy
            case exec_action_type::notify_writer:      writer_event_.set(); break;
            case exec_action_type::wait_for_response:
            {
               auto [ec] = co_await request_done.wait();
               ignore_unused(ec);  // TODO: we should likely use this
               break;
            }
            case exec_action_type::done: co_return {act.error()};
         }
      }
   }

   void set_receive_adapter(any_adapter adapter)
   {
      st_.mpx.set_receive_adapter(std::move(adapter));
   }
};

inline capy::io_task<> receive2(corosio_connection_impl& conn)
{
   // Setup
   receive_fsm fsm;
   system::error_code ec;

   while (true) {
      receive_action act = fsm.resume(
         conn.st_,
         ec,
         token_to_cancel(co_await capy::this_coro::stop_token));

      switch (act.type) {
         case receive_action::action_type::setup_cancellation: break;  // not required here
         case receive_action::action_type::wait:
         {
            auto [controller_ec] = co_await conn.controller_.take();
            ec = controller_ec;
            break;
         }
         case receive_action::action_type::drain_channel: break;  // not required
         case receive_action::action_type::immediate:     break;  // not required
         case receive_action::action_type::done:          co_return {act.ec};
      }
   }
}

inline capy::io_task<> async_exec_one(
   corosio_connection_impl& conn,
   const request& req,
   any_adapter resp)
{
   exec_one_fsm fsm{std::move(resp), req.get_expected_responses()};
   system::error_code ec;
   std::size_t bytes = 0u;

   while (true) {
      exec_one_action act = fsm.resume(
         conn.st_.mpx.get_read_buffer(),
         ec,
         bytes,
         token_to_cancel(co_await capy::this_coro::stop_token));

      switch (act.type) {
         case exec_one_action_type::done: co_return {ec};
         case exec_one_action_type::write:
         {
            auto [write_ec, write_bytes] = co_await capy::write(
               conn.stream_,
               capy::make_buffer(req.payload()));
            ec = write_ec;
            bytes = write_bytes;
            break;
         }
         case exec_one_action_type::read_some:
         {
            auto [read_ec, read_bytes] = co_await conn.stream_.read_some(
               conn.st_.mpx.get_read_buffer().get_prepared());
            ec = read_ec;
            bytes = read_bytes;
            break;
         }
      }
   }
}

inline capy::io_task<> sentinel_resolve(corosio_connection_impl& conn)
{
   // Setup
   sentinel_resolve_fsm fsm;
   system::error_code ec;

   while (true) {
      sentinel_action act = fsm.resume(
         conn.st_,
         ec,
         token_to_cancel(co_await capy::this_coro::stop_token));

      switch (act.get_type()) {
         case sentinel_action::type::done: co_return {act.error()};
         case sentinel_action::type::connect:
         {
            auto [connect_ec] = co_await conn.stream_.connect(
               make_sentinel_connect_params(conn.st_.cfg, act.connect_addr()),
               conn.st_.logger);
            ec = connect_ec;
            break;
         }
         case sentinel_action::type::request:
         {
            auto [exec_ec] = co_await async_exec_one(
               conn,
               conn.st_.cfg.sentinel.setup,
               make_sentinel_adapter(conn.st_),
               asio::cancel_after(
                  conn->reconnect_timer_,  // should be safe to re-use this
                  conn->st_.cfg.sentinel.request_timeout,
                  std::move(self)));
            break;
         }
      }
   }
}

template <class Executor>
struct sentinel_resolve_op {
   connection_impl<Executor>* conn_;
   sentinel_resolve_fsm fsm_;

   explicit sentinel_resolve_op(connection_impl<Executor>& conn)
   : conn_(&conn)
   { }

   template <class Self>
   void operator()(Self& self, system::error_code ec = {})
   { }
};

template <class Executor, class CompletionToken>
auto async_sentinel_resolve(connection_impl<Executor>& conn, CompletionToken&& token)
{
   return asio::async_compose<CompletionToken, void(system::error_code)>(
      sentinel_resolve_op<Executor>{conn},
      token,
      conn);
}

template <class Executor>
struct writer_op {
   connection_impl<Executor>* conn_;
   writer_fsm fsm_;

   explicit writer_op(connection_impl<Executor>& conn) noexcept
   : conn_(&conn)
   { }

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t bytes_written = 0u)
   {
      auto* conn = conn_;  // Prevent potential use-after-move errors with cancel_after
      auto act = fsm_.resume(
         conn->st_,
         ec,
         bytes_written,
         self.get_cancellation_state().cancelled());

      switch (act.type()) {
         case writer_action_type::done: self.complete(act.error()); return;
         case writer_action_type::write_some:
            conn->stream_.async_write_some(
               asio::buffer(conn->st_.mpx.get_write_buffer()),
               asio::cancel_at(
                  conn->writer_timer_,
                  compute_expiry(act.timeout()),
                  std::move(self)));
            return;
         case writer_action_type::wait:
            conn->writer_cv_.expires_at(compute_expiry(act.timeout()));
            conn->writer_cv_.async_wait(std::move(self));
            return;
      }
   }
};

template <class Executor>
struct reader_op {
   connection_impl<Executor>* conn_;
   reader_fsm fsm_;

public:
   reader_op(connection_impl<Executor>& conn) noexcept
   : conn_{&conn}
   { }

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t n = 0)
   {
      for (;;) {
         auto* conn = conn_;  // Prevent potential use-after-move errors with cancel_after
         auto act = fsm_.resume(conn->st_, n, ec, self.get_cancellation_state().cancelled());

         switch (act.get_type()) {
            case reader_fsm::action::type::read_some:
               conn->stream_.async_read_some(
                  asio::buffer(conn->st_.mpx.get_prepared_read_buffer()),
                  asio::cancel_at(
                     conn->reader_timer_,
                     compute_expiry(act.timeout()),
                     std::move(self)));
               return;
            case reader_fsm::action::type::notify_push_receiver:
               if (conn->receive_channel_.try_send(ec, act.push_size())) {
                  continue;
               } else {
                  conn->receive_channel_.async_send(ec, act.push_size(), std::move(self));
               }
               return;
            case reader_fsm::action::type::done: self.complete(act.error()); return;
         }
      }
   }
};

template <class Executor>
class run_op {
private:
   connection_impl<Executor>* conn_;
   run_fsm fsm_{};

   template <class CompletionToken>
   auto reader(CompletionToken&& token)
   {
      return asio::async_compose<CompletionToken, void(system::error_code)>(
         reader_op<Executor>{*conn_},
         std::forward<CompletionToken>(token),
         conn_->writer_cv_);
   }

   template <class CompletionToken>
   auto writer(CompletionToken&& token)
   {
      return asio::async_compose<CompletionToken, void(system::error_code)>(
         writer_op<Executor>{*conn_},
         std::forward<CompletionToken>(token),
         conn_->writer_cv_);
   }

public:
   run_op(connection_impl<Executor>* conn) noexcept
   : conn_{conn}
   { }

   // Called after the parallel group finishes
   template <class Self>
   void operator()(
      Self& self,
      std::array<std::size_t, 2u> order,
      system::error_code reader_ec,
      system::error_code writer_ec)
   {
      (*this)(self, order[0u] == 0u ? reader_ec : writer_ec);
   }

   template <class Self>
   void operator()(Self& self, system::error_code ec = {})
   {
      auto act = fsm_.resume(conn_->st_, ec, self.get_cancellation_state().cancelled());

      switch (act.type) {
         case run_action_type::done: self.complete(act.ec); return;
         case run_action_type::immediate:
            asio::async_immediate(self.get_io_executor(), std::move(self));
            return;
         case run_action_type::sentinel_resolve:
            async_sentinel_resolve(*conn_, std::move(self));
            return;
         case run_action_type::connect:
            conn_->stream_.async_connect(
               make_run_connect_params(conn_->st_),
               conn_->st_.logger,
               std::move(self));
            return;
         case run_action_type::parallel_group:
            asio::experimental::make_parallel_group(
               [this](auto token) {
                  return this->reader(token);
               },
               [this](auto token) {
                  return this->writer(token);
               })
               .async_wait(asio::experimental::wait_for_one(), std::move(self));
            return;
         case run_action_type::cancel_receive:
            conn_->receive_channel_.cancel();
            (*this)(self);  // this action does not require suspending
            return;
         case run_action_type::wait_for_reconnection:
            conn_->reconnect_timer_.expires_after(conn_->st_.cfg.reconnect_wait_interval);
            conn_->reconnect_timer_.async_wait(std::move(self));
            return;
         default: BOOST_ASSERT(false);
      }
   }
};

logger make_stderr_logger(logger::level lvl, std::string prefix);

template <class Executor>
class run_cancel_handler {
   connection_impl<Executor>* conn_;

public:
   explicit run_cancel_handler(connection_impl<Executor>& conn) noexcept
   : conn_(&conn)
   { }

   void operator()(asio::cancellation_type_t cancel_type) const
   {
      // We support terminal and partial cancellation
      constexpr auto mask = asio::cancellation_type_t::terminal |
                            asio::cancellation_type_t::partial;

      if ((cancel_type & mask) != asio::cancellation_type_t::none) {
         conn_->cancel(operation::run);
      }
   }
};

}  // namespace detail

/** @brief A SSL connection to the Redis server.
 *
 *  This class keeps a healthy connection to the Redis instance where
 *  commands can be sent at any time. For more details, please see the
 *  documentation of each individual function.
 *
 *  @tparam Executor The executor type used to create any required I/O objects.
 */
template <class Executor>
class basic_connection {
public:
   using this_type = basic_connection<Executor>;

   /// (Deprecated) Type of the next layer
   BOOST_DEPRECATED("This typedef is deprecated, and will be removed with next_layer().")
   typedef asio::ssl::stream<asio::basic_stream_socket<asio::ip::tcp, Executor>> next_layer_type;

   /// The type of the executor associated to this object.
   using executor_type = Executor;

   /// Rebinds the socket type to another executor.
   template <class Executor1>
   struct rebind_executor {
      /// The connection type when rebound to the specified executor.
      using other = basic_connection<Executor1>;
   };

   /** @brief Constructor from an executor.
   *
   *  @param ex Executor used to create all internal I/O objects.
   *  @param ctx SSL context.
   *  @param lgr Logger configuration. It can be used to filter messages by level
   *             and customize logging. By default, `logger::level::info` messages
   *             and higher are logged to `stderr`.
   */
   explicit basic_connection(
      executor_type ex,
      asio::ssl::context ctx = asio::ssl::context{asio::ssl::context::tlsv12_client},
      logger lgr = {})
   : impl_(
        std::make_unique<detail::connection_impl<Executor>>(
           std::move(ex),
           std::move(ctx),
           std::move(lgr)))
   { }

   /** @brief Constructor from an executor and a logger.
    *
    *  @param ex Executor used to create all internal I/O objects.
    *  @param lgr Logger configuration. It can be used to filter messages by level
    *             and customize logging. By default, `logger::level::info` messages
    *             and higher are logged to `stderr`.
    *
    * An SSL context with default settings will be created.
    */
   basic_connection(executor_type ex, logger lgr)
   : basic_connection(
        std::move(ex),
        asio::ssl::context{asio::ssl::context::tlsv12_client},
        std::move(lgr))
   { }

   /**
    * @brief Constructor from an `io_context`.
    * 
    * @param ioc I/O context used to create all internal I/O objects.
    * @param ctx SSL context.
    * @param lgr Logger configuration. It can be used to filter messages by level
    *            and customize logging. By default, `logger::level::info` messages
    *            and higher are logged to `stderr`.
    */
   explicit basic_connection(
      asio::io_context& ioc,
      asio::ssl::context ctx = asio::ssl::context{asio::ssl::context::tlsv12_client},
      logger lgr = {})
   : basic_connection(ioc.get_executor(), std::move(ctx), std::move(lgr))
   { }

   /**
    * @brief Constructor from an `io_context` and a logger.
    * 
    * @param ioc I/O context used to create all internal I/O objects.
    * @param lgr Logger configuration. It can be used to filter messages by level
    *            and customize logging. By default, `logger::level::info` messages
    *            and higher are logged to `stderr`.
    */
   basic_connection(asio::io_context& ioc, logger lgr)
   : basic_connection(
        ioc.get_executor(),
        asio::ssl::context{asio::ssl::context::tlsv12_client},
        std::move(lgr))
   { }

   /// Returns the associated executor.
   executor_type get_executor() noexcept { return impl_->writer_cv_.get_executor(); }

   /** @brief Starts the underlying connection operations.
    *
    * This function establishes a connection to the Redis server and keeps
    * it healthy by performing the following operations:
    *
    *  @li For Sentinel deployments (`config::sentinel::addresses` is not empty),
    *      contacts Sentinels to obtain the address of the configured master.
    *  @li For TCP connections, resolves the server hostname passed in
    *      @ref boost::redis::config::addr.
    *  @li Establishes a physical connection to the server. For TCP connections,
    *      connects to one of the endpoints obtained during name resolution.
    *      For UNIX domain socket connections, it connects to @ref boost::redis::config::unix_sockets.
    *  @li If @ref boost::redis::config::use_ssl is `true`, performs the TLS handshake.
    *  @li Executes the setup request, as defined by the passed @ref config object.
    *      By default, this is a `HELLO` command, but it can contain any other arbitrary
    *      commands. See the @ref config::setup docs for more info.
    *  @li Starts a health-check operation where `PING` commands are sent
    *      at intervals specified by
    *      @ref config::health_check_interval when the connection is idle.
    *      See the documentation of @ref config::health_check_interval for more info.
    *  @li Starts read and write operations. Requests issued using @ref async_exec
    *      before `async_run` is called will be written to the server immediately.
    *
    *  When a connection is lost for any reason, a new one is
    *  established automatically. To disable reconnection
    *  set @ref boost::redis::config::reconnect_wait_interval to zero.
    *
    *  The completion token must have the following signature
    *
    *  @code
    *  void f(system::error_code);
    *  @endcode
    *
    * @par Per-operation cancellation
    * This operation supports the following cancellation types:
    *
    *   @li `asio::cancellation_type_t::terminal`.
    *   @li `asio::cancellation_type_t::partial`.
    *
    * In both cases, cancellation is equivalent to calling @ref basic_connection::cancel
    * passing @ref operation::run as argument.
    *
    * After the operation completes, the token's associated cancellation slot
    * may still have a cancellation handler associated to this connection.
    * You should make sure to not invoke it after the connection has been destroyed.
    * This is consistent with what other Asio I/O objects do.
    *
    * For example on how to call this function refer to
    * cpp20_intro.cpp or any other example.
    *
    * @param cfg Configuration parameters.
    * @param token Completion token.
    */
   template <class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_run(config const& cfg, CompletionToken&& token = {})
   {
      return asio::async_initiate<CompletionToken, void(system::error_code)>(
         run_initiation{impl_.get()},
         token,
         &cfg);
   }

   /** @brief Wait for server pushes asynchronously.
    *
    * This function suspends until at least one server push is received by the
    * connection. On completion an unspecified number of pushes will
    * have been added to the response object set with @ref
    * set_receive_response. Use the functions in the response object
    * to know how many messages they were received and consume them.
    *
    * To prevent receiving an unbound number of pushes the connection
    * blocks further read operations on the socket when 256 pushes
    * accumulate internally (we don't make any commitment to this
    * exact number). When that happens any `async_exec`s and
    * health-checks won't make any progress and the connection may
    * eventually timeout. To avoid this, apps that expect server pushes
    * should call this function continuously in a loop.
    *
    * This function should be used instead of the deprecated @ref async_receive.
    * It differs from `async_receive` in the following:
    *
    * @li `async_receive` is designed to consume a single push message at a time.
    *     This can be inefficient when receiving lots of server pushes.
    *     `async_receive2` is batch-oriented. All pushes that are available
    *     when `async_receive2` is called will be marked as consumed.
    * @li `async_receive` is cancelled when a reconnection happens (e.g. because
    *     of a network error). This enabled the user to re-establish subscriptions
    *     using @ref async_exec before waiting for pushes again. With the introduction of
    *     functions like @ref request::subscribe, subscriptions are automatically
    *     re-established on reconnection. Thus, `async_receive2` is not cancelled
    *     on reconnection.
    * @li `async_receive` passes the number of bytes that each received
    *     push message contains. This information is unreliable and not very useful.
    *     Equivalent information is available using functions in the response object.
    * @li `async_receive` might get cancelled if `async_run` is cancelled.
    *     This doesn't happen with `async_receive2`.
    *
    * This function does *not* remove messages from the response object
    * passed to @ref set_receive_response - use the functions in the response
    * object to achieve this.
    *
    * Only a single instance of `async_receive2` may be outstanding
    * for a given connection at any time. Trying to start a second one
    * will fail with @ref error::already_running.
    *
    * @note To avoid deadlocks the task (e.g. coroutine) calling
    * `async_receive2` should not call `async_exec` in a way where
    * they could block each other. This is, avoid the following pattern:
    *
    * @code
    * asio::awaitable<void> receiver()
    * {
    *    // Do NOT do this!!! The receive buffer might get full while 
    *    // async_exec runs, which will block all read operations until async_receive2
    *    // is called. The two operations end up waiting each other, making the connection unresponsive.
    *    // If you need to do this, use two connections, instead.
    *    co_await conn.async_receive2();
    *    co_await conn.async_exec(req, resp);
    * }
    * @endcode
    *
    * For an example see cpp20_subscriber.cpp.
    *
    * The completion token must have the following signature:
    *
    * @code
    * void f(system::error_code);
    * @endcode
    *
    * @par Per-operation cancellation
    * This operation supports the following cancellation types:
    *
    *   @li `asio::cancellation_type_t::terminal`.
    *   @li `asio::cancellation_type_t::partial`.
    *   @li `asio::cancellation_type_t::total`.
    *
    * @param token Completion token.
    */
   capy::task<> receive(CompletionToken&& token = {}) { return detail::receive2(*impl_); }

   /** @brief Executes commands on the Redis server asynchronously.
    *
    * This function sends a request to the Redis server and waits for
    * the responses to each individual command in the request. If the
    * request contains only commands that don't expect a response,
    * the completion occurs after it has been written to the
    * underlying stream.  Multiple concurrent calls to this function
    * will be automatically queued by the implementation.
    *
    * For an example see cpp20_echo_server.cpp.
    *
    * The completion token must have the following signature:
    *
    * @code
    * void f(system::error_code, std::size_t);
    * @endcode
    *
    * Where the second parameter is the size of the response received
    * in bytes.
    *
    * @par Per-operation cancellation
    * This operation supports per-operation cancellation. Depending on the state of the request
    * when cancellation is requested, we can encounter two scenarios:
    *
    *   @li If the request hasn't been sent to the server yet, cancellation will prevent it
    *       from being sent to the server. In this situation, all cancellation types are supported
    *       (`asio::cancellation_type_t::terminal`, `asio::cancellation_type_t::partial` and
    *       `asio::cancellation_type_t::total`).
    *   @li If the request has been sent to the server but the response hasn't arrived yet,
    *       cancellation will cause `async_exec` to complete immediately. When the response
    *       arrives from the server, it will be ignored. In this situation, only
    *       `asio::cancellation_type_t::terminal` and `asio::cancellation_type_t::partial`
    *       are supported. Cancellation requests specifying `asio::cancellation_type_t::total`
    *       only will be ignored.
    *
    * In any case, connections can be safely used after cancelling `async_exec` operations.
    *
    * @par Object lifetimes
    * Both `req` and `res` should be kept alive until the operation completes.
    * No copies of the request object are made.
    *
    * @param req The request to be executed.
    * @param resp The response object to parse data into.
    * @param token Completion token.
    */
   template <
      class Response = ignore_t,
      class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_exec(request const& req, Response& resp = ignore, CompletionToken&& token = {})
   {
      return this->async_exec(req, any_adapter{resp}, std::forward<CompletionToken>(token));
   }

   /** @brief Executes commands on the Redis server asynchronously.
    *
    * This function sends a request to the Redis server and waits for
    * the responses to each individual command in the request. If the
    * request contains only commands that don't expect a response,
    * the completion occurs after it has been written to the
    * underlying stream.  Multiple concurrent calls to this function
    * will be automatically queued by the implementation.
    *
    * For an example see cpp20_echo_server.cpp.
    *
    * The completion token must have the following signature:
    *
    * @code
    * void f(system::error_code, std::size_t);
    * @endcode
    *
    * Where the second parameter is the size of the response received
    * in bytes.
    *
    * @par Per-operation cancellation
    * This operation supports per-operation cancellation. Depending on the state of the request
    * when cancellation is requested, we can encounter two scenarios:
    *
    *   @li If the request hasn't been sent to the server yet, cancellation will prevent it
    *       from being sent to the server. In this situation, all cancellation types are supported
    *       (`asio::cancellation_type_t::terminal`, `asio::cancellation_type_t::partial` and
    *       `asio::cancellation_type_t::total`).
    *   @li If the request has been sent to the server but the response hasn't arrived yet,
    *       cancellation will cause `async_exec` to complete immediately. When the response
    *       arrives from the server, it will be ignored. In this situation, only
    *       `asio::cancellation_type_t::terminal` and `asio::cancellation_type_t::partial`
    *       are supported. Cancellation requests specifying `asio::cancellation_type_t::total`
    *       only will be ignored.
    *
    * In any case, connections can be safely used after cancelling `async_exec` operations.
    *
    * @par Object lifetimes
    * Both `req` and any response object referenced by `adapter`
    * should be kept alive until the operation completes.
    * No copies of the request object are made.
    *
    * @param req The request to be executed.
    * @param adapter An adapter object referencing a response to place data into.
    * @param token Completion token.
    */
   template <class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_exec(request const& req, any_adapter adapter, CompletionToken&& token = {})
   {
      return impl_->async_exec(req, std::move(adapter), std::forward<CompletionToken>(token));
   }

   /** @brief Cancel operations.
    *
    *  @li `operation::exec`: cancels operations started with
    *  `async_exec`. Affects only requests that haven't been written
    *  yet.
    *  @li `operation::run`: cancels the `async_run` operation.
    *  @li `operation::receive`: cancels any ongoing calls to `async_receive`.
    *  @li `operation::all`: cancels all operations listed above.
    *
    *  @param op The operation to be cancelled.
    */
   void cancel(operation op = operation::all) { impl_->cancel(op); }

   /// Returns true if the connection will try to reconnect if an error is encountered.
   bool will_reconnect() const noexcept { return impl_->will_reconnect(); }

   /// Sets the response object of @ref async_receive2 operations.
   template <class Response>
   void set_receive_response(Response& resp)
   {
      impl_->set_receive_adapter(any_adapter{resp});
   }

   /// Returns connection usage information.
   usage get_usage() const noexcept { return impl_->st_.mpx.get_usage(); }

private:
   using clock_type = std::chrono::steady_clock;
   using clock_traits_type = asio::wait_traits<clock_type>;
   using timer_type = asio::basic_waitable_timer<clock_type, clock_traits_type, executor_type>;

   using receive_channel_type = asio::experimental::channel<
      executor_type,
      void(system::error_code, std::size_t)>;

   auto use_ssl() const noexcept { return impl_->cfg_.use_ssl; }

   // Used by both this class and connection
   void set_stderr_logger(logger::level lvl, const config& cfg)
   {
      impl_->st_.logger.lgr = detail::make_stderr_logger(lvl, cfg.log_prefix);
   }

   // Initiation for async_run. This is required because we need access
   // to the final handler (rather than the completion token) within the initiation,
   // to modify the handler's cancellation slot.
   struct run_initiation {
      detail::connection_impl<Executor>* self;

      using executor_type = Executor;
      executor_type get_executor() const noexcept { return self->get_executor(); }

      template <class Handler>
      void operator()(Handler&& handler, config const* cfg)
      {
         self->st_.cfg = *cfg;
         self->st_.mpx.set_config(*cfg);

         // If the token's slot has cancellation enabled, it should just emit
         // the cancellation signal in our connection. This lets us unify the cancel()
         // function and per-operation cancellation
         auto slot = asio::get_associated_cancellation_slot(handler);
         if (slot.is_connected()) {
            slot.template emplace<detail::run_cancel_handler<Executor>>(*self);
         }

         // Overwrite the token's cancellation slot: the composed operation
         // should use the signal's slot so we can generate cancellations in cancel()
         auto token_with_slot = asio::bind_cancellation_slot(
            self->run_signal_.slot(),
            std::forward<Handler>(handler));

         asio::async_compose<decltype(token_with_slot), void(system::error_code)>(
            detail::run_op<Executor>{self},
            token_with_slot,
            self->writer_cv_);
      }
   };

   friend class connection;

   std::unique_ptr<detail::connection_impl<Executor>> impl_;
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_CONNECTION_HPP
