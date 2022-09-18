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
 *  This class provides a wrapper to the `Connection` types passed as
 *  template parameter that provides synchronous communication to
 *  Redis.
 */
template <class Connection>
class sync:
   public sync_base<
      typename Connection::executor_type,
      sync<Connection>> {
public:
   /// Next layer type.
   using next_layer_type = Connection;

   /// Config options from the underlying connection.
   using config = typename next_layer_type::config;

   /// Operation options from the underlying connection.
   using operation = typename next_layer_type::operation;

   /// The executor type of the underlying connection.
   using executor_type = typename next_layer_type::executor_type;

   /// Returns the executor used in the underlying connection.
   auto get_executor() noexcept { return conn_.get_executor();}

   /** @brief Constructor
    *  
    *  @param ex Executor
    *  @param cfg Config options.
    */
   explicit sync(executor_type ex, config cfg = config{}) : conn_{ex, cfg} { }

   /** @brief Constructor
    *  
    *  @param ioc The io_context.
    *  @param cfg Config options.
    */
   explicit sync(boost::asio::io_context& ioc, config cfg = config{})
   : sync(ioc, std::move(cfg))
   { }

   /// Returns a reference to the next layer.
   auto& next_layer() noexcept { return conn_; }

   /// Returns a const reference to the next layer.
   auto next_layer() const noexcept -> auto const& { return conn_; }

private:
   template <class, class> friend class sync_base;
   Connection conn_;
};

} // aedis

#endif // AEDIS_SYNC_HPP
