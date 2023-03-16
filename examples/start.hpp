/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_EXAMPLES_START_HPP
#define BOOST_REDIS_EXAMPLES_START_HPP

#include <boost/asio/awaitable.hpp>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

auto start(boost::asio::awaitable<void> op) -> int;

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
#endif // BOOST_REDIS_EXAMPLES_START_HPP
