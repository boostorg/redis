/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_SSL_CONNECTION_HPP
#define AEDIS_SSL_CONNECTION_HPP

#include <chrono>
#include <memory>

#include <boost/asio/io_context.hpp>
#include <aedis/connection_base.hpp>

namespace aedis::ssl {

namespace detail
{

#include <boost/asio/yield.hpp>

template <class Stream>
struct handshake_op {
   Stream* stream;
   aedis::detail::conn_timer_t<typename Stream::executor_type>* timer;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , std::array<std::size_t, 2> order = {}
                  , boost::system::error_code ec1 = {}
                  , boost::system::error_code ec2 = {})
   {
      reenter (coro)
      {
         yield
         boost::asio::experimental::make_parallel_group(
            [this](auto token)
            {
               return stream->async_handshake(boost::asio::ssl::stream_base::client, token);
            },
            [this](auto token) { return timer->async_wait(token);}
         ).async_wait(
            boost::asio::experimental::wait_for_one(),
            std::move(self));

         switch (order[0]) {
            case 0: self.complete(ec1); return;
            case 1:
            {
               BOOST_ASSERT_MSG(!ec2, "handshake_op: Incompatible state.");
               self.complete(error::ssl_handshake_timeout);
               return;
            }

            default: BOOST_ASSERT(false);
         }
      }
   }
};

template <
   class Stream,
   class CompletionToken
   >
auto async_handshake(
      Stream& stream,
      aedis::detail::conn_timer_t<typename Stream::executor_type>& timer,
      CompletionToken&& token)
{
   return boost::asio::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(handshake_op<Stream>{&stream, &timer}, token, stream, timer);
}

template <class Conn>
struct ssl_connect_with_timeout_op {
   Conn* conn = nullptr;
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , boost::asio::ip::tcp::endpoint const& = {})
   {
      reenter (coro)
      {
         conn->ping_timer_.expires_after(conn->get_config().connect_timeout);

         yield
         aedis::detail::async_connect(
            conn->lowest_layer(), conn->ping_timer_, conn->endpoints_, std::move(self));

         if (ec) {
            self.complete(ec);
            return;
         }

         conn->ping_timer_.expires_after(conn->get_config().handshake_timeout);

         yield
         async_handshake(conn->next_layer(), conn->ping_timer_, std::move(self));
         self.complete(ec);
      }
   }
};

#include <boost/asio/unyield.hpp>

} // detail
 
template <class>
class connection;

/** @brief Connection to the Redis server over SSL sockets.
 *  @ingroup any
 */
template <class AsyncReadWriteStream>
class connection<boost::asio::ssl::stream<AsyncReadWriteStream>> :
   public connection_base<
      typename boost::asio::ssl::stream<AsyncReadWriteStream>::executor_type,
      connection<boost::asio::ssl::stream<AsyncReadWriteStream>>> {
public:
   /// Type of the next layer
   using next_layer_type = boost::asio::ssl::stream<AsyncReadWriteStream>;

   /// Executor type.
   using executor_type = typename next_layer_type::executor_type;

   using base_type = connection_base<executor_type, connection<boost::asio::ssl::stream<AsyncReadWriteStream>>>;
   using this_type = connection<next_layer_type>;

   /** @brief Connection configuration parameters.
    */
   struct config {
      /// Timeout of the resolve operation.
      std::chrono::milliseconds resolve_timeout = std::chrono::seconds{10};

      /// Timeout of the connect operation.
      std::chrono::milliseconds connect_timeout = std::chrono::seconds{10};

      /// Timeout of the ssl handshake operation.
      std::chrono::milliseconds handshake_timeout = std::chrono::seconds{10};

      /// Time interval of ping operations.
      std::chrono::milliseconds ping_interval = std::chrono::seconds{1};

      /// The maximum size of read operations.
      std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)();

      /// Whether to coalesce requests (see [pipelines](https://redis.io/topics/pipelining)).
      bool coalesce_requests = true;
   };

   /// Constructor
   explicit connection(executor_type ex, boost::asio::ssl::context& ctx, config cfg = {})
   : base_type{ex}
   , cfg_{cfg}
   , stream_{ex, ctx}
   {
   }

   explicit connection(boost::asio::io_context& ioc, boost::asio::ssl::context& ctx, config cfg = config{})
   : connection(ioc.get_executor(), ctx, std::move(cfg))
   { }

   /// Get the config object.
   auto get_config() noexcept -> config& { return cfg_;}

   /// Gets the config object.
   auto get_config() const noexcept -> config const& { return cfg_;}

   /// Reset the underlying stream.
   void reset_stream(boost::asio::ssl::context& ctx)
   {
      stream_ = next_layer_type{ex_, ctx};
   }

   auto& next_layer() noexcept { return stream_; }
   auto const& next_layer() const noexcept { return stream_; }

   // TODO: Make this private.
   void close() { stream_.next_layer().close(); }
   auto& lowest_layer() noexcept { return stream_.lowest_layer(); }
   auto is_open() const noexcept { return stream_.next_layer().is_open(); }

   template <class CompletionToken>
   auto async_connect(CompletionToken&& token)
   {
      return boost::asio::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::ssl_connect_with_timeout_op<this_type>{this}, token, stream_);
   }

private:
   template <class> friend struct detail::ssl_connect_with_timeout_op;

   config cfg_;
   executor_type ex_;
   next_layer_type stream_;
};

} // aedis::ssl

#endif // AEDIS_SSL_CONNECTION_HPP
