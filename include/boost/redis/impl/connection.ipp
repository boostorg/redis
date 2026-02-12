/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>

#include <boost/assert.hpp>
#include <boost/core/ignore_unused.hpp>

#include <cstddef>
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
capy::io_task<Types...> cancel_at(
   capy::io_task<Types...> task,
   corosio::timer& timer,
   std::chrono::steady_clock::time_point timeout)
{
   timer.expires_at(timeout);
   auto [winner_index, result] = co_await capy::when_any(std::move(task), timer.wait());
   if (winner_index == 0u)
      co_return std::get<0>(std::move(result));
   else
      co_return {make_error_code(capy::error::canceled)};
}

template <class... Types>
capy::io_task<Types...> cancel_after(
   capy::io_task<Types...> task,
   corosio::timer& timer,
   std::chrono::steady_clock::duration timeout)
{
   return cancel_at(std::move(task), timer, std::chrono::steady_clock::now() + timeout);
}

capy::io_task<> corosio_redis_stream::connect(const connect_params& params, buffered_logger& l)
{
   connect_fsm fsm{l};
   system::error_code ec;
   corosio::resolver_results endpoints;

   auto act = fsm.resume(ec, st_);

   while (true) {
      switch (act.type) {
         case connect_action_type::unix_socket_close:
            BOOST_ASSERT(false);
            co_return {std::make_error_code(std::errc::operation_not_supported)};
         case connect_action_type::unix_socket_connect:
            BOOST_ASSERT(false);
            co_return {std::make_error_code(std::errc::operation_not_supported)};
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
            socket_.close();
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

corosio_connection_impl::corosio_connection_impl(
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

void corosio_connection_impl::cancel()
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

capy::io_task<> corosio_connection_impl::exec(request const& req, any_adapter adapter)
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

void corosio_connection_impl::set_receive_adapter(any_adapter adapter)
{
   st_.mpx.set_receive_adapter(std::move(adapter));
}

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
            break;
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

}  // namespace detail

connection::connection(capy::execution_context& ctx, corosio::tls_context ssl_ctx, logger lgr)
: impl_(std::make_unique<detail::corosio_connection_impl>(ctx, std::move(ssl_ctx), std::move(lgr)))
{ }

connection::connection(capy::execution_context& ctx, logger lgr)
: connection(ctx, {}, std::move(lgr))
{ }

capy::io_task<> connection::run(config const& cfg)
{
   impl_->st_.cfg = cfg;
   impl_->st_.mpx.set_config(cfg);
   impl_->run_cancelled_event_.clear();

   auto [winner_index, result] = co_await capy::when_any(
      detail::run(*impl_),
      impl_->run_cancelled_event_.wait());

   co_return {winner_index == 0u ? std::get<0>(result).ec : capy::error::canceled};
}

capy::io_task<> connection::receive() { return detail::receive(*impl_); }

}  // namespace boost::redis
