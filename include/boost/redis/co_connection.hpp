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
#include <boost/redis/logger.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>
#include <boost/redis/usage.hpp>

#include <boost/capy/ex/execution_context.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/corosio/tls_context.hpp>

#include <memory>
#include <utility>

namespace boost::redis {
namespace detail {

struct co_connection_impl;

}  // namespace detail

/** @brief A connection to a Redis server, designed for capy coroutines.
 *
 * This class keeps a healthy connection to the Redis instance where
 * commands can be sent at any time. For more details, please see the
 * documentation of each individual function.
 *
 * Each I/O member function returns a `capy::io_task` that should
 * be awaited from a coroutine running on a capy executor.
 * This class uses corosio for sockets, TLS and timers, and does not depend on Boost.Asio.
 * Cancellation follows the usual capy patterns, and is driven by the parent task's
 * `std::stop_token`. When stop is requested, operations complete with
 * an error matching `capy::cond::canceled`.
 *
 * This type is movable but not copyable.
 *
 * @par Thread safety
 * Distinct objects: safe.
 * Shared objects: unsafe.
 * This class is <b>not thread-safe</b>: for a single object, if you
 * call its member functions concurrently from separate threads, you will get a race condition.
 * Use a strand if if you require thread safety.
 */
class co_connection {
public:
   /** @brief Constructor from an execution context.
    *
    *  @param ctx Execution context used to create all internal I/O objects.
    *  @param tls_ctx TLS context, used to create any required TLS streams.
    *                 Used only when @ref config::use_ssl is `true`.
    *                 The connection is usable with TLS even if not specified -
    *                 a default-constructed TLS context is used in this case.
    *  @param lgr Logger configuration. It can be used to filter messages by level
    *             and customize logging. By default, `logger::level::info` messages
    *             and higher are logged to `stderr`.
    */
   explicit co_connection(
      capy::execution_context& ctx,
      corosio::tls_context tls_ctx = {},
      logger lgr = {});

   /** @brief Constructor from a capy executor.
    *
    *  Equivalent to constructing from `ex.context()`.
    *
    *  @param ex Executor whose context will own this connection.
    *  @param tls_ctx TLS context, used to create any required TLS streams.
    *                 Used only when @ref config::use_ssl is `true`.
    *                 The connection is usable with TLS even if not specified -
    *                 a default-constructed TLS context is used in this case.
    *  @param lgr Logger configuration. It can be used to filter messages by level
    *             and customize logging. By default, `logger::level::info` messages
    *             and higher are logged to `stderr`.
    */
   template <capy::Executor Ex>
   co_connection(const Ex& ex, corosio::tls_context tls_ctx = {}, logger lgr = {})
   : co_connection(ex.context(), std::move(tls_ctx), std::move(lgr))
   { }

   /** @brief Constructor from an execution context and a logger.
    *
    *  A TLS context with default settings will be created.
    *
    *  @param ctx Execution context used to create all internal I/O objects.
    *  @param lgr Logger configuration. It can be used to filter messages by level
    *             and customize logging. By default, `logger::level::info` messages
    *             and higher are logged to `stderr`.
    */
   co_connection(capy::execution_context& ctx, logger lgr);

   /** @brief Constructor from a capy executor and a logger.
    *
    *  Equivalent to constructing from `ex.context()`.
    *  A TLS context with default settings will be created.
    *
    *  @param ex Executor whose context will own this connection.
    *  @param lgr Logger configuration. It can be used to filter messages by level
    *             and customize logging. By default, `logger::level::info` messages
    *             and higher are logged to `stderr`.
    */
   template <capy::Executor Ex>
   co_connection(const Ex& ex, logger lgr)
   : co_connection(ex.context(), corosio::tls_context{}, std::move(lgr))
   { }

   /** @brief Move constructor.
    *
    *  Transfers ownership of the connection's internal state from `other` to
    *  the new object. Any operations that were in flight on `other` continue to make
    *  progress on the same internal state, now owned by `*this`.
    *
    *  After the move, `other` is in a valid but unspecified state,
    *  and can only be assigned to or destroyed.
    *
    *  @par Exception safety
    *  No-throw guarantee.
    */
   co_connection(co_connection&&) noexcept;

   /** @brief Move assignment operator.
    *
    *  Releases the resources currently owned by `*this` (if any) and
    *  transfers ownership of `other`'s internal state to `*this`.
    *
    *  Any operations that were in flight on `other` continue to make
    *  progress on the same internal state, now owned by `*this`.
    *  If `*this` has any operation in flight, the behavior is undefined.
    *
    *  After the move, `other` is in a valid but unspecified state,
    *  and can only be assigned to or destroyed.
    *
    *  @par Exception safety
    *  No-throw guarantee.
    *
    *  @return Reference to `*this`.
    */
   co_connection& operator=(co_connection&&) noexcept;

   /// Destructor.
   ~co_connection();

   /** @brief Starts the underlying connection operations.
    *
    * This function establishes a connection to the Redis server and keeps
    * it healthy by performing the following operations:
    *
    *  @li For Sentinel deployments (`config::sentinel::addresses` is not empty),
    *      contacts Sentinels to obtain the address of the configured master.
    *  @li For TCP connections, resolves the server hostname passed in
    *      @ref config::addr.
    *  @li Establishes a physical connection to the server. For TCP connections,
    *      connects to one of the endpoints obtained during name resolution.
    *      For UNIX domain socket connections, it connects to @ref config::unix_socket.
    *  @li If @ref config::use_ssl is `true`, performs the TLS handshake.
    *  @li Executes the setup request, as defined by the passed @ref config object.
    *      By default, this is a `HELLO` command, but it can contain any other arbitrary
    *      commands. See the @ref config::setup docs for more info.
    *  @li Starts a health-check operation where `PING` commands are sent
    *      at intervals specified by
    *      @ref config::health_check_interval when the connection is idle.
    *      See the documentation of @ref config::health_check_interval for more info.
    *  @li Starts read and write operations. Requests submitted via @ref exec
    *      before this task is ready to send data will be queued and written to
    *      the server as soon as the connection is up.
    *
    * When a connection is lost for any reason, a new one is
    * established automatically. To disable reconnection
    * set @ref config::reconnect_wait_interval to zero.
    *
    * Awaiting the returned task yields a `capy::io_result` containing
    * a single `std::error_code`:
    *
    * @code
    * auto [ec] = co_await conn.run(cfg);
    * @endcode
    *
    * If the passed configuration contains a critical error
    * (e.g. TLS is enabled together with UNIX sockets),
    * the operation completes immediately with a non-empty error code.
    *
    * If reconnection is diabled, the operation completes
    * once an event that would otherwise trigger a reconnection is encountered.
    * An informative error code is returned.
    *
    * If reconnection is enabled, the operation only completes when cancelled.
    *
    * @par Cancellation
    * The usual capy cancellation semantics apply. The operation can be used
    * in constructs like `capy::timeout` and `capy::with_any`.
    *
    * For an example on how to call this function refer to
    * corosio_intro.cpp or any other corosio example.
    *
    * @param cfg Configuration parameters.
    *
    * @return A task that yields a `capy::io_result` holding the
    *         operation's `std::error_code` on completion.
    */
   capy::io_task<> run(config const& cfg);

   /** @brief Wait for server pushes.
    *
    * This function suspends until at least one server push is received by the
    * connection. On completion, an unspecified number of pushes will have been
    * added to the response object set with @ref set_receive_response. Use the
    * functions in the response object to know how many messages were received
    * and consume them.
    *
    * To prevent receiving an unbounded number of pushes, the connection blocks
    * further read operations on the socket when its internal receive buffer
    * fills up. When that happens, in-flight @ref exec calls and health checks
    * won't make any progress and the connection may eventually time out. To
    * avoid this, applications that expect server pushes should call this
    * function continuously in a loop.
    *
    * This function does *not* remove messages from the response object
    * passed to @ref set_receive_response. Use the functions in the response
    * object to achieve this.
    *
    * Only a single instance of `receive` may be outstanding for a given
    * connection at any time. Launching a second `receive` fails
    * with @ref error::already_running.
    *
    * `receive` does not complete when reconnection happens or
    * when @ref run completes.
    *
    * @note To avoid deadlocks, the task calling `receive` should not also call
    * `exec` in a way where they can block each other. That is, avoid the
    * following pattern:
    *
    * @code
    * capy::task<void> receiver(co_connection& conn)
    * {
    *    // Do NOT do this!!! The receive buffer might get full while
    *    // exec runs, which will block all read operations until receive
    *    // is called. The two operations end up waiting for each other,
    *    // making the connection unresponsive. If you need this pattern,
    *    // use two connections instead.
    *    co_await conn.receive();
    *    co_await conn.exec(req, resp);
    * }
    * @endcode
    *
    * For an example see corosio_subscriber.cpp.
    *
    * Awaiting the returned task yields a `capy::io_result` containing
    * a single `std::error_code`:
    *
    * @code
    * auto [ec] = co_await conn.receive();
    * @endcode
    *
    * @par Cancellation
    * The usual capy cancellation semantics apply. The operation can be used
    * in constructs like `capy::timeout` and `capy::with_any`.
    *
    * @return A task that yields a `capy::io_result` holding the
    *         operation's `std::error_code` on completion.
    */
   capy::io_task<> receive();

   /** @brief Executes commands on the Redis server.
    *
    * This function sends a request to the Redis server and waits for
    * the responses to each individual command in the request. If the
    * request contains only commands that don't expect a response,
    * the completion occurs after it has been written to the underlying
    * stream. Multiple concurrent calls to this function will be
    * automatically queued by the implementation.
    *
    * For an example see corosio_intro.cpp.
    *
    * Awaiting the returned task yields a `capy::io_result` containing
    * a single `std::error_code`:
    *
    * @code
    * auto [ec] = co_await conn.exec(req, resp);
    * @endcode
    *
    * @par Cancellation
    * The usual capy cancellation semantics apply. The operation can be used
    * in constructs like `capy::timeout` and `capy::with_any`.
    *
    * What happens to the request depends on its state when cancellation is requested:
    *
    *   @li If the request hasn't been sent to the server yet, cancellation will
    *       prevent it from being sent.
    *   @li If the request has been sent but the response hasn't arrived yet,
    *       cancellation causes `exec` to complete immediately. When the response
    *       eventually arrives from the server, it will be ignored.
    *
    * @par Object lifetimes
    * Both `req` and `resp` should be kept alive until the operation completes.
    * No copies of the request object are made. After `exec` completes, the objects
    * can be safely destroyed, even if `exec` was cancelled.
    *
    * @param req The request to be executed.
    * @param resp The response object to parse data into.
    *
    * @return A task that yields a `capy::io_result` holding the
    *         operation's `std::error_code` on completion.
    */
   template <class Response = ignore_t>
   capy::io_task<> exec(request const& req, Response& resp = ignore)
   {
      return exec(req, any_adapter{resp});
   }

   /** @brief Executes commands on the Redis server.
    *
    * This is the type-erased version of `exec`.
    * It has the same semantics as the typed `exec` overload
    * Same as the other @ref exec overload, but takes a type-erased
    * @ref any_adapter instead of a typed response.
    *
    * This function sends a request to the Redis server and waits for
    * the responses to each individual command in the request. If the
    * request contains only commands that don't expect a response,
    * the completion occurs after it has been written to the underlying
    * stream. Multiple concurrent calls to this function will be
    * automatically queued by the implementation.
    *
    * For an example see corosio_intro.cpp.
    *
    * Awaiting the returned task yields a `capy::io_result` containing
    * a single `std::error_code`:
    *
    * @code
    * auto [ec] = co_await conn.exec(req, resp);
    * @endcode
    *
    * @par Cancellation
    * The usual capy cancellation semantics apply. The operation can be used
    * in constructs like `capy::timeout` and `capy::with_any`.
    *
    * What happens to the request depends on its state when cancellation is requested:
    *
    *   @li If the request hasn't been sent to the server yet, cancellation will
    *       prevent it from being sent.
    *   @li If the request has been sent but the response hasn't arrived yet,
    *       cancellation causes `exec` to complete immediately. When the response
    *       eventually arrives from the server, it will be ignored.
    *
    * @par Object lifetimes
    * Both `req` and any response object referenced by `adapter`
    * should be kept alive until the operation completes.
    * No copies of the request object are made.
    *
    * @param req The request to be executed.
    * @param adapter An adapter object referencing a response to place data into.
    *
    * @return A task that yields a @ref boost::capy::io_result holding the
    *         operation's `std::error_code` on completion.
    */
   capy::io_task<> exec(request const& req, any_adapter adapter);

   /**
    * @brief Sets the response object for @ref receive operations.
    *
    * Pushes received by the connection (concretely, by @ref run)
    * will be stored in `resp`. This happens even if @ref receive
    * is not being called.
    *
    * `resp` should be able to accommodate the following message types:
    *
    *  @li Any kind of RESP3 pushes that the application might expect.
    *      This usually involves Pub/Sub messages of type `message`,
    *      `subscribe`, `unsubscribe`, `psubscribe` and `punsubscribe`.
    *      See <a href="https://redis.io/docs/latest/develop/pubsub/">this page</a>
    *      for more info.
    *  @li Any errors caused by failed `SUBSCRIBE` commands. Because of protocol
    *      oddities, these are placed in the receive buffer rather than handed to
    *      @ref exec.
    *  @li If your application is using `MONITOR`, simple strings.
    *
    * Because receive responses need to accommodate many different kinds
    * of messages, it's advised to use one of the generic responses
    * (like @ref generic_flat_response). If a response can't accommodate
    * one of the received types, @ref run will exit with an error.
    *
    * Messages received before this function is called are discarded.
    *
    * @par Object lifetimes
    * `resp` should be kept alive until @ref run completes.
    */
   template <class Response>
   void set_receive_response(Response& resp)
   {
      set_receive_adapter(any_adapter(resp));
   }

   /// Returns connection usage information.
   usage get_usage() const noexcept;

private:
   void set_receive_adapter(any_adapter adapter);

   std::unique_ptr<detail::co_connection_impl> impl_;
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_CO_CONNECTION_HPP
