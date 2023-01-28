/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RESP3_TYPE_HPP
#define BOOST_REDIS_RESP3_TYPE_HPP

#include <ostream>
#include <vector>
#include <string>

namespace boost::redis::resp3 {

/** \brief RESP3 data types.
    \ingroup high-level-api
  
    The RESP3 specification can be found at https://github.com/redis/redis-specifications/blob/master/protocol/RESP3.md.
 */
enum class type
{ /// Aggregate
  array,
  /// Aaggregate
  push,
  /// Aggregate
  set,
  /// Aggregate
  map,
  /// Aggregate
  attribute,
  /// Simple
  simple_string,
  /// Simple
  simple_error,
  /// Simple
  number,
  /// Simple
  doublean,
  /// Simple
  boolean,
  /// Simple
  big_number,
  /// Simple
  null,
  /// Simple
  blob_error,
  /// Simple
  verbatim_string,
  /// Simple
  blob_string,
  /// Simple
  streamed_string,
  /// Simple
  streamed_string_part,
  /// Invalid
  invalid
};

/** \brief Converts the data type to a string.
 *  \ingroup high-level-api
 *  \param t RESP3 type.
 */
auto to_string(type t) -> char const*;

/** \brief Writes the type to the output stream.
 *  \ingroup high-level-api
 *  \param os Output stream.
 *  \param t RESP3 type.
 */
auto operator<<(std::ostream& os, type t) -> std::ostream&;

/* Checks whether the data type is an aggregate.
 */
auto is_aggregate(type t) -> bool;

// For map and attribute data types this function returns 2.  All
// other types have value 1.
auto element_multiplicity(type t) -> std::size_t;

// Returns the wire code of a given type.
auto to_code(type t) -> char;

// Converts a wire-format RESP3 type (char) to a resp3 type.
auto to_type(char c) -> type;

} // boost::redis::resp3

#endif // BOOST_REDIS_RESP3_TYPE_HPP
