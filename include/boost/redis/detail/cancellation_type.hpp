//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_CANCELLATION_TYPE_HPP
#define BOOST_REDIS_CANCELLATION_TYPE_HPP

// A minimal port of asio::cancellation_type_t to avoid
// depending on Asio in the protocol state machines

namespace boost::redis::detail {

enum class cancellation_type : int
{
   none = 0,
   terminal = 1,
   partial = 2,
   total = 4,
};

constexpr bool contains(cancellation_type value, cancellation_type query)
{
   return (static_cast<int>(value) & static_cast<int>(query)) != 0;
}

constexpr bool contains_terminal(cancellation_type value)
{
   return contains(value, cancellation_type::terminal);
}

constexpr bool contains_partial(cancellation_type value)
{
   return contains(value, cancellation_type::partial);
}

constexpr bool contains_total(cancellation_type value)
{
   return contains(value, cancellation_type::total);
}

}  // namespace boost::redis::detail

#endif
