//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_CO_CONNECTION_HPP
#define BOOST_REDIS_CO_CONNECTION_HPP

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

class co_redis_stream {
   // TODO: UNIX sockets
   corosio::tcp_socket socket_;
   corosio::openssl_stream stream_;  // TODO: make this configurable
   corosio::timer timer_;
   corosio::resolver resolv_;
   redis_stream_state st_;

public:
   explicit co_redis_stream(capy::execution_context& ctx, corosio::tls_context tls_ctx)
   : socket_(ctx)
   , stream_(&socket_, std::move(tls_ctx))
   , timer_(ctx)
   , resolv_(ctx)
   { }

   // I/O
   capy::io_task<> connect(const connect_params& params, buffered_logger& l);

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

struct co_connection_impl {
   capy::async_event run_cancelled_event_;
   co_redis_stream stream_;
   corosio::timer writer_timer_;     // timer used for write timeouts
   corosio::timer writer_cv_;        // set when there is new data to write
   corosio::timer reader_timer_;     // timer used for read timeouts
   corosio::timer reconnect_timer_;  // to wait the reconnection period
   corosio::timer ping_timer_;       // to wait between pings
   flow_controller controller_;
   connection_state st_;

   co_connection_impl(capy::execution_context& ctx, corosio::tls_context&& ssl_ctx, logger&& lgr);

   void cancel();

   capy::io_task<> exec(request const& req, any_adapter adapter);

   void set_receive_adapter(any_adapter adapter);
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
class co_connection {
public:
   /** @brief Constructor from an executor.
   *
   *  @param ex Executor used to create all internal I/O objects.
   *  @param ctx SSL context.
   *  @param lgr Logger configuration. It can be used to filter messages by level
   *             and customize logging. By default, `logger::level::info` messages
   *             and higher are logged to `stderr`.
   */
   explicit co_connection(
      capy::execution_context& ctx,
      corosio::tls_context ssl_ctx = {},
      logger lgr = {});

   template <capy::Executor Ex>
   co_connection(const Ex& ex, corosio::tls_context ssl_ctx = {}, logger lgr = {})
   : co_connection(ex.context(), std::move(ssl_ctx), std::move(lgr))
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
   co_connection(capy::execution_context& ctx, logger lgr);

   template <capy::Executor Ex>
   co_connection(const Ex& ex, logger lgr)
   : co_connection(ex.context(), corosio::tls_context{}, std::move(lgr))
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
   capy::io_task<> run(config const& cfg);

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
   capy::io_task<> receive();

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
   std::unique_ptr<detail::co_connection_impl> impl_;
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_CONNECTION_HPP
