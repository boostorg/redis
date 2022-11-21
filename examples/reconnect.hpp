/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_EXAMPLES_RECONNECT_HPP
#define AEDIS_EXAMPLES_RECONNECT_HPP

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <aedis.hpp>
#include <memory>

using connection = boost::asio::use_awaitable_t<>::as_default_on_t<aedis::connection>;

// TODO: Remove reconnect and let people loop over run.
// Connects to a Redis instance. If use_sentinel is true, the master
// address is resolved using a sentinel, more info in
// - https://redis.io/docs/manual/sentinel.
// - https://redis.io/docs/reference/sentinel-clients.
auto
reconnect(
   std::shared_ptr<connection> conn,
   aedis::resp3::request req,
   bool use_sentinel) -> boost::asio::awaitable<void>;

auto run(std::shared_ptr<connection> conn) -> boost::asio::awaitable<void>;

auto healthy_checker(std::shared_ptr<connection> conn) -> boost::asio::awaitable<void>;

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
#endif // AEDIS_EXAMPLES_RECONNECT_HPP
