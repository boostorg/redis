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

/// RESP3 message types.
enum class type
{ array
, push
, set
, map
, attribute
, simple_string
, simple_error
, number
, doublean
, boolean
, big_number
, null
, blob_error
, verbatim_string
, blob_string
, streamed_string_part
, invalid
};

/// Converts a RESP3 type to a string.
std::string to_string(type t);

// TODO: Move to detail?
type to_type(char c);

/// Writes the string representation of type to the output stream.
std::ostream& operator<<(std::ostream& os, type t);

} // resp3
} // aedis
