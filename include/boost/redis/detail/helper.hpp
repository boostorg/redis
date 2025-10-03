/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_HELPER_HPP
#define BOOST_REDIS_HELPER_HPP

#include <boost/asio/cancellation_type.hpp>

namespace boost::redis::detail {

template <class T>
auto is_cancelled(T const& self)
{
   return self.get_cancellation_state().cancelled() != asio::cancellation_type_t::none;
}

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_HELPER_HPP
