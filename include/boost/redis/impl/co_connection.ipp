//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/co_connection.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/detail/cancellation_type.hpp>
#include <boost/redis/detail/connect_params.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/exec_fsm.hpp>
#include <boost/redis/detail/exec_one_fsm.hpp>
#include <boost/redis/detail/flow_controller.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/reader_fsm.hpp>
#include <boost/redis/detail/receive_fsm.hpp>
#include <boost/redis/detail/run_fsm.hpp>
#include <boost/redis/detail/sentinel_resolve_fsm.hpp>
#include <boost/redis/detail/transport_type.hpp>
#include <boost/redis/detail/writer_fsm.hpp>
#include <boost/redis/impl/co_connect.hpp>

#include <boost/assert.hpp>
#include <boost/capy/buffers.hpp>
#include <boost/capy/buffers/make_buffer.hpp>
#include <boost/capy/concept/io_awaitable.hpp>
#include <boost/capy/cond.hpp>
#include <boost/capy/ex/async_event.hpp>
#include <boost/capy/ex/execution_context.hpp>
#include <boost/capy/ex/this_coro.hpp>
#include <boost/capy/io/any_stream.hpp>
#include <boost/capy/io_result.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/timeout.hpp>
#include <boost/capy/when_any.hpp>
#include <boost/capy/write.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/corosio/connect.hpp>
#include <boost/corosio/local_stream_socket.hpp>
#include <boost/corosio/openssl_stream.hpp>
#include <boost/corosio/resolver.hpp>
#include <boost/corosio/resolver_results.hpp>
#include <boost/corosio/tcp_socket.hpp>
#include <boost/corosio/timer.hpp>
#include <boost/corosio/tls_context.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <stop_token>
#include <system_error>
#include <utility>
#include <variant>

namespace boost::redis {
namespace detail {

// Given a timeout value, compute the expiry time. A zero timeout is considered to mean "no timeout"
inline std::chrono::steady_clock::time_point compute_expiry(
   std::chrono::steady_clock::duration timeout)
{
   return timeout.count() == 0 ? (std::chrono::steady_clock::time_point::max)()
                               : std::chrono::steady_clock::now() + timeout;
}

inline cancellation_type to_cancel(std::stop_token tok)
{
   return tok.stop_requested() ? cancellation_type::terminal : cancellation_type::none;
}

// Run an operation with a timeout, with a zero timeout meaning 'no timeout'
template <class Aw>
capy::task<capy::awaitable_result_t<Aw>> maybe_timeout(
   Aw aw,
   std::chrono::steady_clock::duration timeout)
{
   if (timeout.count() == 0)
      co_return co_await std::move(aw);
   else
      co_return co_await capy::timeout(std::move(aw), timeout);
}

struct co_redis_stream {
   struct tcp_state {
      corosio::resolver resolv;
      corosio::tcp_socket sock;

      explicit tcp_state(capy::execution_context& ctx)
      : resolv(ctx)
      , sock(ctx)
      { }
   };

   // Required to create the other objects
   capy::execution_context& ctx_;
   corosio::tls_context tls_ctx_;

   // Constructed lazily as required
   std::optional<tcp_state> tcp_;
   std::optional<corosio::local_stream_socket> unix_;
   std::optional<corosio::openssl_stream> tls_;

   // Contains the stream that will end up being used
   capy::any_stream stream_;

   void setup_tcp_impl()
   {
      // Allocate the object if not there.
      // TCP uses range connect, so we don't need to close and reopen the socket
      if (!tcp_.has_value())
         tcp_.emplace(ctx_);
   }

   void setup_unix()
   {
      if (unix_.has_value()) {
         // UNIX sockets don't use range connect.
         // We need to close and re-open the socket before establishing another connection
         unix_->close();
         unix_->open();
      } else {
         unix_.emplace(ctx_);
      }
      stream_ = capy::any_stream(&*unix_);
   }

   void setup_tcp()
   {
      setup_tcp_impl();
      stream_ = capy::any_stream(&tcp_->sock);
   }

   void setup_tcp_tls()
   {
      setup_tcp_impl();
      if (tls_.has_value())
         tls_->reset();
      else
         tls_.emplace(capy::any_stream(&tcp_->sock), tls_ctx_);
      stream_ = capy::any_stream(&*tls_);
   }

   auto unix_connect(const connect_params& params)
   {
      return capy::timeout(
         unix_->connect(corosio::local_endpoint(params.addr.unix_socket())),
         params.connect_timeout);
   }

   auto tcp_resolve(const connect_params& params)
   {
      return capy::timeout(
         tcp_->resolv.resolve(params.addr.tcp_address().host, params.addr.tcp_address().port),
         params.resolve_timeout);
   }

   auto tcp_connect(const connect_params& params, const corosio::resolver_results& results)
   {
      // TODO: prevent copy here
      return capy::timeout(corosio::connect(tcp_->sock, results), params.connect_timeout);
   }

   auto tls_handshake(const connect_params& params)
   {
      return capy::timeout(
         tls_->handshake(corosio::tls_stream::handshake_type::client),
         params.ssl_handshake_timeout);
   }

   capy::io_task<> connect(const connect_params& params, buffered_logger& lgr)
   {
      return co_connect(*this, params, lgr);
   }

   explicit co_redis_stream(capy::execution_context& ctx, corosio::tls_context tls_ctx)
   : ctx_(ctx)
   , tls_ctx_(std::move(tls_ctx))
   { }

   template <capy::ConstBufferSequence BuffType>
   auto write_some(BuffType&& buffers)
   {
      return stream_.write_some(std::forward<BuffType>(buffers));
   }

   template <capy::MutableBufferSequence BuffType>
   auto read_some(BuffType&& buffers)
   {
      return stream_.read_some(std::forward<BuffType>(buffers));
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

   co_connection_impl(capy::execution_context& ctx, corosio::tls_context&& ssl_ctx, logger&& lgr)
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

   void set_receive_adapter(any_adapter adapter)
   {
      st_.mpx.set_receive_adapter(std::move(adapter));
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
         auto act = fsm.resume(true, st_, to_cancel(co_await capy::this_coro::stop_token));

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

   capy::io_task<> receive()
   {
      // Setup
      receive_fsm fsm;
      system::error_code ec;

      while (true) {
         receive_action act = fsm.resume(st_, ec, to_cancel(co_await capy::this_coro::stop_token));

         switch (act.type) {
            case receive_action::action_type::setup_cancellation: break;  // not required here
            case receive_action::action_type::wait:
            {
               auto [controller_ec] = co_await controller_.take();
               ec = controller_ec;
               break;
            }
            case receive_action::action_type::drain_channel: break;  // not required
            case receive_action::action_type::immediate:     break;  // not required
            case receive_action::action_type::done:          co_return {act.ec};
         }
      }
   }

   capy::io_task<> exec_one(const request& req, any_adapter resp)
   {
      exec_one_fsm fsm{std::move(resp), req.get_expected_responses()};
      auto& rdbuff = st_.mpx.get_read_buffer();

      // First invocation
      auto act = fsm.resume(rdbuff, system::error_code(), 0u, cancellation_type::none);

      while (true) {
         switch (act.type) {
            case exec_one_action_type::done: co_return {act.ec};
            case exec_one_action_type::write:
            {
               auto [ec, bytes] = co_await capy::write(stream_, capy::make_buffer(req.payload()));
               act = fsm.resume(rdbuff, ec, bytes, to_cancel(co_await capy::this_coro::stop_token));
               break;
            }
            case exec_one_action_type::read_some:
            {
               // https://github.com/cppalliance/capy/issues/147
               auto buff = rdbuff.get_prepared();
               auto [ec, bytes] = co_await stream_.read_some(
                  capy::mutable_buffer(buff.data(), buff.size()));
               act = fsm.resume(rdbuff, ec, bytes, to_cancel(co_await capy::this_coro::stop_token));
               break;
            }
         }
      }
   }

   capy::io_task<> sentinel_resolve()
   {
      // Setup
      sentinel_resolve_fsm fsm;
      auto act = fsm.resume(st_, system::error_code(), cancellation_type::none);

      while (true) {
         switch (act.get_type()) {
            case sentinel_action::type::done: co_return {act.error()};
            case sentinel_action::type::connect:
            {
               auto [ec] = co_await stream_.connect(
                  make_sentinel_connect_params(st_.cfg, act.connect_addr()),
                  st_.logger);
               act = fsm.resume(st_, ec, to_cancel(co_await capy::this_coro::stop_token));
               break;
            }
            case sentinel_action::type::request:
            {
               auto [ec] = co_await capy::timeout(
                  exec_one(st_.cfg.sentinel.setup, make_sentinel_adapter(st_)),
                  st_.cfg.sentinel.request_timeout);
               act = fsm.resume(st_, ec, to_cancel(co_await capy::this_coro::stop_token));
               break;
            }
         }
      }
   }

   // This signature is required because capy::when_any is equivalent to wait_for_one_success
   capy::io_task<std::error_code> writer()
   {
      // Setup.
      writer_fsm fsm{capy::cond::timeout};
      auto act = fsm.resume(st_, system::error_code(), 0u, cancellation_type::none);

      while (true) {
         switch (act.type()) {
            case writer_action_type::done: co_return {{}, act.error()};
            case writer_action_type::write_some:
            {
               auto [ec, bytes] = co_await maybe_timeout(
                  stream_.write_some(capy::make_buffer(st_.mpx.get_write_buffer())),
                  act.timeout());
               act = fsm.resume(st_, ec, bytes, to_cancel(co_await capy::this_coro::stop_token));
               break;
            }
            case writer_action_type::wait:
            {
               writer_cv_.expires_at(compute_expiry(act.timeout()));
               auto [ec] = co_await writer_cv_.wait();
               act = fsm.resume(st_, ec, 0u, to_cancel(co_await capy::this_coro::stop_token));
               break;
            }
         }
      }
   }

   capy::io_task<std::error_code> reader()
   {
      reader_fsm fsm{capy::cond::timeout};
      auto act = fsm.resume(st_, 0u, system::error_code(), cancellation_type::none);

      for (;;) {
         switch (act.get_type()) {
            case reader_fsm::action::type::read_some:
            {
               // https://github.com/cppalliance/capy/issues/147
               auto buff = st_.mpx.get_prepared_read_buffer();
               auto [ec, bytes] = co_await maybe_timeout(
                  stream_.read_some(capy::mutable_buffer(buff.data(), buff.size())),
                  act.timeout());
               act = fsm.resume(st_, bytes, ec, to_cancel(co_await capy::this_coro::stop_token));
               break;
            }
            case reader_fsm::action::type::notify_push_receiver:
            {
               auto cancel = to_cancel(co_await capy::this_coro::stop_token);
               if (controller_.try_put(act.push_size())) {
                  act = fsm.resume(st_, 0u, system::error_code(), cancel);
               } else {
                  auto [ec] = co_await controller_.put(act.push_size());
                  act = fsm.resume(st_, 0u, ec, cancel);
               }
               break;
            }
            case reader_fsm::action::type::done: co_return {{}, act.error()};
         }
      }
   }

   capy::io_task<> run(const config& cfg)
   {
      // corosio only runs in systems that support UNIX sockets
      constexpr bool unix_sockets_supported = true;
      run_fsm fsm{unix_sockets_supported};
      system::error_code ec;

      // Setup
      st_.cfg = cfg;
      st_.mpx.set_config(cfg);

      while (true) {
         auto act = fsm.resume(st_, ec, to_cancel(co_await capy::this_coro::stop_token));

         switch (act.type) {
            case run_action_type::done:             co_return {act.ec};
            case run_action_type::sentinel_resolve: ec = (co_await sentinel_resolve()).ec; break;
            case run_action_type::connect:
               ec = (co_await stream_.connect(make_run_connect_params(st_), st_.logger)).ec;
               break;
            case run_action_type::parallel_group:
            {
               auto result = co_await capy::when_any(reader(), writer());
               ec = std::visit(
                  [](std::error_code value) {
                     return value;
                  },
                  result);
               break;
            }
            case run_action_type::cancel_receive:
            case run_action_type::immediate:
               ec = system::error_code();
               break;  // no longer required
            case run_action_type::wait_for_reconnection:
               reconnect_timer_.expires_after(st_.cfg.reconnect_wait_interval);
               ec = (co_await reconnect_timer_.wait()).ec;
               break;
         }
      }
   }
};

}  // namespace detail

co_connection::co_connection(capy::execution_context& ctx, corosio::tls_context ssl_ctx, logger lgr)
: impl_(std::make_unique<detail::co_connection_impl>(ctx, std::move(ssl_ctx), std::move(lgr)))
{ }

co_connection::co_connection(capy::execution_context& ctx, logger lgr)
: co_connection(ctx, {}, std::move(lgr))
{ }

co_connection::co_connection(co_connection&&) noexcept = default;
co_connection& co_connection::operator=(co_connection&&) noexcept = default;
co_connection::~co_connection() = default;

capy::io_task<> co_connection::run(config const& cfg) { return impl_->run(cfg); }

capy::io_task<> co_connection::receive() { return impl_->receive(); }

capy::io_task<> co_connection::exec(request const& req, any_adapter adapter)
{
   return impl_->exec(req, std::move(adapter));
}

void co_connection::set_receive_adapter(any_adapter resp)
{
   impl_->set_receive_adapter(std::move(resp));
}

usage co_connection::get_usage() const noexcept { return impl_->st_.mpx.get_usage(); }

}  // namespace boost::redis
