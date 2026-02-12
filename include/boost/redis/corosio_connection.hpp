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
#include <boost/asio/error.hpp>
#include <boost/assert.hpp>
#include <boost/capy/buffers.hpp>
#include <boost/capy/buffers/make_buffer.hpp>
#include <boost/capy/error.hpp>
#include <boost/capy/ex/async_event.hpp>
#include <boost/capy/ex/execution_context.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io_result.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/capy/write.hpp>
#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/corosio/openssl_stream.hpp>
#include <boost/corosio/resolver.hpp>
#include <boost/corosio/resolver_results.hpp>
#include <boost/corosio/tcp_socket.hpp>
#include <boost/corosio/timer.hpp>
#include <boost/corosio/tls_context.hpp>
#include <boost/corosio/tls_stream.hpp>
#include <boost/system/detail/error_code.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <stop_token>
#include <string>
#include <system_error>
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
   return tok.stop_requested() ? asio::cancellation_type_t::terminal
                               : asio::cancellation_type_t::none;
}

template <class... Types>
inline capy::io_task<Types...> cancel_at(
   capy::io_task<Types...> task,
   corosio::timer& timer,
   std::chrono::steady_clock::time_point timeout)
{
   timer.expires_at(timeout);
   auto [winner_index, result] = co_await capy::when_any(std::move(task), timer.wait());
   if (winner_index == 0u)
      co_return std::get<0>(std::move(result));
   else
      co_return {make_error_code(asio::error::operation_aborted)};
}

template <class... Types>
inline capy::io_task<Types...> cancel_after(
   capy::io_task<Types...> task,
   corosio::timer& timer,
   std::chrono::steady_clock::duration timeout)
{
   return cancel_at(std::move(task), timer, std::chrono::steady_clock::now() + timeout);
}

class corosio_redis_stream {
   // TODO: UNIX sockets
   corosio::tcp_socket socket_;
   corosio::openssl_stream stream_;  // TODO: make this configurable
   corosio::timer timer_;
   corosio::resolver resolv_;
   redis_stream_state st_;

public:
   explicit corosio_redis_stream(capy::execution_context& ctx, corosio::tls_context tls_ctx)
   : socket_(ctx)
   , stream_(&socket_, std::move(tls_ctx))
   , timer_(ctx)
   , resolv_(ctx)
   { }

   // I/O
   capy::io_task<> connect(const connect_params& params, buffered_logger& l)
   {
      connect_fsm fsm{l};
      system::error_code ec;
      corosio::resolver_results endpoints;

      auto act = fsm.resume(ec, st_);

      while (true) {
         switch (act.type) {
            case connect_action_type::unix_socket_close:
               BOOST_ASSERT(false);
               co_return {system::error_code(asio::error::operation_not_supported)};
            case connect_action_type::unix_socket_connect:
               BOOST_ASSERT(false);
               co_return {system::error_code(asio::error::operation_not_supported)};
            case connect_action_type::tcp_resolve:
            {
               auto result = co_await cancel_after(
                  [&] -> capy::io_task<corosio::resolver_results> {
                     co_return co_await resolv_.resolve(
                        params.addr.tcp_address().host,
                        params.addr.tcp_address().port);
                  }(),
                  timer_,
                  params.resolve_timeout);
               ec = result.ec;
               endpoints = std::move(result.t1);
               act = fsm.resume(ec, endpoints, st_);
               break;
            }
            case connect_action_type::ssl_stream_reset:
               stream_.reset();
               act = fsm.resume(ec, st_);
               break;
            case connect_action_type::ssl_handshake:
               ec = (co_await cancel_after(
                        stream_.handshake(corosio::tls_stream::handshake_type::client),
                        timer_,
                        params.ssl_handshake_timeout))
                       .ec;
               act = fsm.resume(ec, st_);
               break;
            case connect_action_type::done: co_return {act.ec};
            case connect_action_type::tcp_connect:
            {
               // TODO: range connect
               socket_.open();
               auto result = co_await cancel_after(
                  [&] -> capy::io_task<> {
                     co_return co_await socket_.connect(*endpoints.begin());
                  }(),
                  timer_,
                  params.connect_timeout);
               ec = result.ec;
               act = fsm.resume(ec, *endpoints.begin(), st_);
               break;
            }
            default: BOOST_ASSERT(false);
         }
      }
   }

   template <capy::ConstBufferSequence BuffType>
   capy::io_task<std::size_t> write_some(const BuffType& buffers)
   {
      switch (st_.type) {
         case transport_type::tcp:         co_return co_await socket_.write_some(buffers);
         case transport_type::tcp_tls:     co_return co_await stream_.write_some(buffers);
         case transport_type::unix_socket:
         default:                          BOOST_ASSERT(false); co_return {};
      }
   }

   template <capy::MutableBufferSequence BuffType>
   capy::io_task<std::size_t> read_some(const BuffType& buffers)
   {
      switch (st_.type) {
         case transport_type::tcp:         co_return co_await socket_.read_some(buffers);
         case transport_type::tcp_tls:     co_return co_await stream_.read_some(buffers);
         case transport_type::unix_socket:
         default:                          BOOST_ASSERT(false); co_return {};
      }
   }
};

struct corosio_connection_impl {
   capy::async_event run_cancelled_event_;
   corosio_redis_stream stream_;
   corosio::timer writer_timer_;     // timer used for write timeouts
   corosio::timer writer_cv_;        // set when there is new data to write
   corosio::timer reader_timer_;     // timer used for read timeouts
   corosio::timer reconnect_timer_;  // to wait the reconnection period
   corosio::timer ping_timer_;       // to wait between pings
   flow_controller controller_;
   connection_state st_;

   corosio_connection_impl(
      capy::execution_context& ctx,
      corosio::tls_context&& ssl_ctx,
      logger&& lgr)
   : stream_{ctx, std::move(ssl_ctx)}
   , writer_timer_{ctx}
   , writer_cv_{ctx}
   , reader_timer_{ctx}
   , reconnect_timer_{ctx}
   , ping_timer_{ctx}
   , controller_{1024u * 1024u * 16u}  // 16MB, TODO: make it configurable
   , st_{{std::move(lgr)}}
   {
      set_receive_adapter(any_adapter{ignore});
      writer_cv_.expires_at((std::chrono::steady_clock::time_point::max)());
   }

   void cancel()
   {
      // exec
      st_.mpx.cancel_waiting();

      // receive (TODO: do we really need this?)
      st_.receive2_cancelled = true;

      // reconnect (TODO: do we really need this?)
      st_.cfg.reconnect_wait_interval = std::chrono::seconds::zero();

      // run
      run_cancelled_event_.set();
   }

   capy::io_task<> exec(request const& req, any_adapter adapter)
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
            case exec_action_type::notify_writer:      writer_cv_.cancel(); break;
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

inline capy::io_task<> receive(corosio_connection_impl& conn)
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
               capy::make_buffer(conn.st_.mpx.get_read_buffer().get_prepared()));
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
            auto [request_ec] = co_await cancel_after(
               async_exec_one(conn, conn.st_.cfg.sentinel.setup, make_sentinel_adapter(conn.st_)),
               conn.reconnect_timer_,
               conn.st_.cfg.sentinel.request_timeout);
            ec = request_ec;
            break;
         }
      }
   }
}

inline capy::io_task<> writer(corosio_connection_impl& conn)
{
   // Setup
   writer_fsm fsm;
   system::error_code ec;
   std::size_t bytes_written = 0u;

   while (true) {
      writer_action act = fsm.resume(
         conn.st_,
         ec,
         bytes_written,
         token_to_cancel(co_await capy::this_coro::stop_token));

      switch (act.type()) {
         case writer_action_type::done: co_return {act.error()};
         case writer_action_type::write_some:
         {
            auto [write_ec, write_bytes] = co_await cancel_at(
               conn.stream_.write_some(capy::make_buffer(conn.st_.mpx.get_write_buffer())),
               conn.writer_timer_,
               compute_expiry(act.timeout()));
            ec = write_ec;
            bytes_written = write_bytes;
            break;
         }
         case writer_action_type::wait:
         {
            conn.writer_cv_.expires_at(compute_expiry(act.timeout()));
            auto [wait_ec] = co_await conn.writer_cv_.wait();
            ec = wait_ec;
            bytes_written = 0u;
            break;
         }
      }
   }
}

inline capy::io_task<> reader(corosio_connection_impl& conn)
{
   reader_fsm fsm;
   std::size_t n = 0u;
   system::error_code ec;

   for (;;) {
      auto act = fsm.resume(conn.st_, n, ec, token_to_cancel(co_await capy::this_coro::stop_token));

      switch (act.get_type()) {
         case reader_fsm::action::type::read_some:
         {
            auto [read_ec, read_bytes] = co_await cancel_at(
               conn.stream_.read_some(capy::make_buffer(conn.st_.mpx.get_prepared_read_buffer())),
               conn.reader_timer_,
               compute_expiry(act.timeout()));
            ec = read_ec;
            n = read_bytes;
            break;
         }
         case reader_fsm::action::type::notify_push_receiver:
         {
            // TODO: re-work this
            auto [notify_ec] = co_await conn.controller_.wait_for_space();
            if (notify_ec)
               ec = notify_ec;
            else
               conn.controller_.put(act.push_size());
         }
         case reader_fsm::action::type::done: co_return {act.error()};
      }
   }
}

inline capy::io_task<> run(corosio_connection_impl& conn)
{
   run_fsm fsm;
   system::error_code ec;

   while (true) {
      auto act = fsm.resume(conn.st_, ec, token_to_cancel(co_await capy::this_coro::stop_token));

      switch (act.type) {
         case run_action_type::done:             co_return {act.ec};
         case run_action_type::immediate:        break;  // no longer required
         case run_action_type::sentinel_resolve: ec = (co_await sentinel_resolve(conn)).ec; break;
         case run_action_type::connect:
            ec = (co_await conn.stream_.connect(make_run_connect_params(conn.st_), conn.st_.logger))
                    .ec;
            break;
         case run_action_type::parallel_group:
         {
            auto [winner_index, result] = co_await capy::when_any(reader(conn), writer(conn));
            ignore_unused(winner_index);
            ec = std::get<0>(result).ec;
            break;
         }
         case run_action_type::cancel_receive: break;  // no longer required
         case run_action_type::wait_for_reconnection:
            conn.reconnect_timer_.expires_after(conn.st_.cfg.reconnect_wait_interval);
            ec = (co_await conn.reconnect_timer_.wait()).ec;
            break;
      }
   }
}

logger make_stderr_logger(logger::level lvl, std::string prefix);

}  // namespace detail

/** @brief A SSL connection to the Redis server.
 *
 *  This class keeps a healthy connection to the Redis instance where
 *  commands can be sent at any time. For more details, please see the
 *  documentation of each individual function.
 *
 *  @tparam Executor The executor type used to create any required I/O objects.
 */
class connection {
public:
   /** @brief Constructor from an executor.
   *
   *  @param ex Executor used to create all internal I/O objects.
   *  @param ctx SSL context.
   *  @param lgr Logger configuration. It can be used to filter messages by level
   *             and customize logging. By default, `logger::level::info` messages
   *             and higher are logged to `stderr`.
   */
   explicit connection(
      capy::execution_context& ctx,
      corosio::tls_context ssl_ctx = {},
      logger lgr = {})
   : impl_(
        std::make_unique<detail::corosio_connection_impl>(ctx, std::move(ssl_ctx), std::move(lgr)))
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
   connection(capy::execution_context& ctx, logger lgr)
   : connection(ctx, {}, std::move(lgr))
   { }

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
   capy::io_task<> run(config const& cfg)
   {
      impl_->st_.cfg = cfg;
      impl_->st_.mpx.set_config(cfg);
      impl_->run_cancelled_event_.clear();

      auto [winner_index, result] = co_await capy::when_any(
         detail::run(*impl_),
         impl_->run_cancelled_event_.wait());

      co_return {winner_index == 0u ? std::get<0>(result).ec : capy::error::canceled};
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
   capy::io_task<> receive() { return detail::receive(*impl_); }

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
   template <class Response = ignore_t>
   capy::io_task<> exec(request const& req, Response& resp = ignore)
   {
      return exec(req, any_adapter{resp});
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
   capy::io_task<> exec(request const& req, any_adapter adapter)
   {
      return impl_->exec(req, std::move(adapter));
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
   void cancel() { impl_->cancel(); }

   /// Sets the response object of @ref async_receive2 operations.
   void set_receive_response(any_adapter resp) { impl_->set_receive_adapter(std::move(resp)); }

   /// Returns connection usage information.
   usage get_usage() const noexcept { return impl_->st_.mpx.get_usage(); }

private:
   // Used by both this class and connection
   void set_stderr_logger(logger::level lvl, const config& cfg)
   {
      impl_->st_.logger.lgr = detail::make_stderr_logger(lvl, cfg.log_prefix);
   }

   std::unique_ptr<detail::corosio_connection_impl> impl_;
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_CONNECTION_HPP
