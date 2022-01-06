/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <ostream>
#include <vector>
#include <string>

namespace aedis {
namespace resp3 {

/** \brief RESP3 types
    \ingroup enums
  
    The RESP3 full specification can be found at https://github.com/antirez/RESP3/blob/74adea588783e463c7e84793b325b088fe6edd1c/spec.md
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

/** \brief Returns the string representation of the type.
 *  \ingroup functions
 *  \param t RESP3 type.
 */
char const* to_string(type t);

/** \brief Writes the type to the output stream.
 *  \ingroup operators
 *  \param os Output stream.
 *  \param t RESP3 type.
 */
std::ostream& operator<<(std::ostream& os, type t);

/** \brief Returns true if the data type is an aggregate.
 *  \ingroup functions
 *  \param t RESP3 type.
 */
bool is_aggregate(type t);

/** @brief Returns the element multilicity.
 *  \ingroup functions
 *  \param t RESP3 type.
 *
 *  For type map and attribute this value is 2, all other types have
 *  1.
 */
std::size_t element_multiplicity(type t);

} // resp3
} // aedis
