/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_EXAMPLES_COMMON_HPP
#define BOOST_REDIS_EXAMPLES_COMMON_HPP

#include <iostream>
#include <boost/asio.hpp>
#include <boost/redis.hpp>
#include <memory>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <string>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

using connection = boost::asio::use_awaitable_t<>::as_default_on_t<boost::redis::connection>;

auto
connect(
   std::shared_ptr<connection> conn,
   std::string const& host,
   std::string const& port) -> boost::asio::awaitable<void>;

auto run(boost::asio::awaitable<void> op) -> int;

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
#endif // BOOST_REDIS_EXAMPLES_COMMON_HPP
