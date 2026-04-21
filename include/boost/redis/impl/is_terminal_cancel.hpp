//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_IS_TERMINAL_CANCEL_HPP
#define BOOST_REDIS_IS_TERMINAL_CANCEL_HPP

#include <boost/redis/detail/cancellation_type.hpp>

namespace boost::redis::detail {

constexpr bool is_terminal_cancel(cancellation_type cancel_state)
{
   return contains_terminal(cancel_state);
}

}  // namespace boost::redis::detail

#endif
