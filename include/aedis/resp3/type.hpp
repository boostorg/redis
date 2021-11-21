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

/** \file type.hpp
    \brief Enum that describes the redis data types and some helper functions.
  
    This file contains the enum used to identify the redis data type
    and some helper functions.
  
    The RESP3 specification can be found at https://github.com/antirez/RESP3/blob/74adea588783e463c7e84793b325b088fe6edd1c/spec.md
 */
enum class type
{ /// Array data type (aggregate).
  array,
  /// Push data type (aggregate).
  push,
  /// Set data type (aggregate).
  set,
  /// Map data type (aggregate).
  map,
  /// Attribute data type (aggregate).
  attribute,
  /// Simple-string data type.
  simple_string,
  /// Simple-error data type.
  simple_error,
  /// Number data type.
  number,
  /// Double data type.
  doublean,
  /// Boolean data type.
  boolean,
  /// Big-number data type.
  big_number,
  /// Null data type.
  null,
  /// Blob-error data type.
  blob_error,
  /// Verbatim-string data type.
  verbatim_string,
  /// Blob-string data type.
  blob_string,
  /// Streamed-string-part data type.
  streamed_string_part,
  /// Represents an invalid data type.
  invalid,
};

/// Returns the string representation of the type.
char const* to_string(type t);

/// Writes the type to the output stream.
std::ostream& operator<<(std::ostream& os, type t);

/// Returns true if the data type is an aggregate.
bool is_aggregate(type t);

/** @brief Returns the element multilicity.
 *
 *  For type map and attribute this value is 2, all other types have 1.
*/
std::size_t element_multiplicity(type t);

} // resp3
} // aedis
