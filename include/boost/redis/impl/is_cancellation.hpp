//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_IS_CANCELLATION_HPP
#define BOOST_REDIS_IS_CANCELLATION_HPP

#include <boost/asio/cancellation_type.hpp>

namespace boost::redis::detail {

inline bool is_cancellation(asio::cancellation_type_t type)
{
   return !!(
      type & (asio::cancellation_type_t::total | asio::cancellation_type_t::partial |
              asio::cancellation_type_t::terminal));
}

}  // namespace boost::redis::detail

#endif
