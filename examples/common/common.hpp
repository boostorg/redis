/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_EXAMPLES_COMMON_HPP
#define AEDIS_EXAMPLES_COMMON_HPP

#include <boost/asio.hpp>
#include <aedis.hpp>
#include <memory>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <string>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

using connection = boost::asio::use_awaitable_t<>::as_default_on_t<aedis::connection>;

auto
connect(
   std::shared_ptr<connection> conn,
   std::string const& host,
   std::string const& port) -> boost::asio::awaitable<void>;

auto healthy_checker(std::shared_ptr<connection> conn) -> boost::asio::awaitable<void>;

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
#endif // AEDIS_EXAMPLES_COMMON_HPP
