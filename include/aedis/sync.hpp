/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_SYNC_HPP
#define AEDIS_SYNC_HPP

#include <aedis/sync_base.hpp>

namespace aedis {

/** @brief High level synchronous connection to Redis.
 *  @ingroup any
 *
 *  The functionality in this class is implemented in the base class.
 */
template <class Connection>
class sync:
   public sync_base<
      typename Connection::executor_type,
      sync<Connection>> {
public:
   // Next layer type.
   using next_layer_type = Connection;

   /// Config options from the underlying connection.
   using config = typename next_layer_type::config;

   /// Operation options from the underlying connection.
   using operation = typename next_layer_type::operation;

   /// The executor type of the underlysing connection.
   using executor_type = typename next_layer_type::executor_type;

   // Return the executor used in the underlying connection.
   auto get_executor() noexcept { return conn_.get_executor();}

   /** @brief Constructor
    *  
    *  @param ex Executor
    *  @param cfg Config options.
    */
   explicit sync(executor_type ex, config cfg = config{}) : conn_{ex, cfg} { }

   /** @brief Constructor
    *  
    *  @param ex The io_context.
    *  @param cfg Config options.
    */
   explicit sync(boost::asio::io_context& ioc, config cfg = config{})
   : sync(ioc, std::move(cfg))
   { }

   /// Returns a reference to the next layer.
   auto& next_layer() noexcept { return conn_; }

   /// Returns a const reference to the next layer.
   auto const& next_layer() const noexcept { return conn_; }

private:
   template <class, class> friend class sync_base;
   Connection conn_;
};

} // aedis

#endif // AEDIS_SYNC_HPP
