/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RUN_HPP
#define BOOST_REDIS_RUN_HPP

// Has to included before promise.hpp to build on msvc.
#include <boost/redis/detail/runner.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/address.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/consign.hpp>
#include <memory>
#include <chrono>

namespace boost::redis {

/** @brief Call async_run on the connection.
 *  @ingroup high-level-api
 *
 *  This is a facility function that
 *  1. Resoves the endpoint.
 *  2. Connects to one of the endpoints from 1.
 *  3. Calls async_run on the underlying connection.
 *
 *  @param conn A connection to Redis.
 *  @param host Redis host to connect to.
 *  @param port Redis port to connect to.
 *  @param resolve_timeout Time the resolve operation is allowed to take.
 *  @param connect_timeout Time the connect operation is allowed to take.
 *  @param token Completion token.
 */
template <
   class Socket,
   class CompletionToken = asio::default_completion_token_t<typename Socket::executor_type>
>
auto
async_run(
   basic_connection<Socket>& conn,
   address addr = address{"127.0.0.1", "6379"},
   std::chrono::steady_clock::duration resolve_timeout = std::chrono::seconds{10},
   std::chrono::steady_clock::duration connect_timeout = std::chrono::seconds{10},
   CompletionToken token = CompletionToken{})
{
   using executor_type = typename Socket::executor_type;
   using runner_type = detail::runner<executor_type>;
   auto runner = std::make_shared<runner_type>(conn.get_executor(), addr);

   return
      runner->async_run(
         conn,
         resolve_timeout,
         connect_timeout,
         asio::consign(std::move(token), runner));
}

} // boost::redis

#endif // BOOST_REDIS_RUN_HPP
