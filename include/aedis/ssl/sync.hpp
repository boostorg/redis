/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_SSL_SYNC_HPP
#define AEDIS_SSL_SYNC_HPP

#include <aedis/sync_base.hpp>
#include <aedis/ssl/connection.hpp>
#include <boost/asio/io_context.hpp>

namespace aedis::ssl {

/** @brief A high level synchronous connection to Redis.
 *  @ingroup any
 *
 *  The functionality in this class is implemented in the base class.
 */
template <class>
class sync;

template <class AsyncReadWriteStream>
class sync<connection<boost::asio::ssl::stream<AsyncReadWriteStream>>> :
   public sync_base<
      typename connection<boost::asio::ssl::stream<AsyncReadWriteStream>>::executor_type,
      sync<connection<boost::asio::ssl::stream<AsyncReadWriteStream>>>> {
public:
   /// Next layer type.
   using next_layer_type = connection<boost::asio::ssl::stream<AsyncReadWriteStream>>;

   /// The executor type of the underlysing connection.
   using executor_type = typename next_layer_type::executor_type;

   /// Config options from the underlying connection.
   using config = typename next_layer_type::config;

   /// Operation options from the underlying connection.
   using operation = typename next_layer_type::operation;

   auto get_executor() noexcept { return conn_.get_executor();}

   /** @brief Constructor
    *  
    *  @param ex Executor
    *  @param cfg Config options.
    */
   explicit sync(executor_type ex, boost::asio::ssl::context& ctx, config cfg = config{})
   : conn_{ex, ctx, cfg}
   { }

   /** @brief Constructor
    *  
    *  @param ex The io_context.
    *  @param cfg Config options.
    */
   explicit sync(boost::asio::io_context& ioc, boost::asio::ssl::context& ctx, config cfg = config{})
   : sync(ioc, ctx, std::move(cfg))
   { }

   auto& next_layer() noexcept { return conn_; }
   auto const& next_layer() const noexcept { return conn_; }

private:
   template <class, class> friend class sync_base;
   next_layer_type conn_;
};

} // aedis

#endif // AEDIS_SSL_SYNC_HPP
