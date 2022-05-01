/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_RESP3_TYPE_HPP
#define AEDIS_RESP3_TYPE_HPP

#include <ostream>
#include <vector>
#include <string>

namespace aedis {
namespace resp3 {

/** \brief RESP3 data types.
    \ingroup any
  
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
  streamed_string_part,
  /// Invalid
  invalid
};

/** \brief Converts the data type to a string.
 *  \ingroup any
 *  \param t RESP3 type.
 */
char const* to_string(type t);

/** \brief Writes the type to the output stream.
 *  \ingroup any
 *  \param os Output stream.
 *  \param t RESP3 type.
 */
std::ostream& operator<<(std::ostream& os, type t);

/* Checks whether the data type is an aggregate.
 */
bool is_aggregate(type t);

// For map and attribute data types this function returns 2.  All
// other types have value 1.
std::size_t element_multiplicity(type t);

// Returns the wire code of a given type.
char to_code(type t);

// Converts a wire-format RESP3 type (char) to a resp3 type.
type to_type(char c);

} // resp3
} // aedis

#endif // AEDIS_RESP3_TYPE_HPP
