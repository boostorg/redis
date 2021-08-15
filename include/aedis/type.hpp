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

#include "command.hpp"

namespace aedis { namespace resp3 {

enum class type
{ array
, push
, set
, map
, attribute
, simple_string
, simple_error
, number
, double_type
, boolean
, big_number
, null
, blob_error
, verbatim_string
, blob_string
, streamed_string_part
, invalid
};

type to_type(char c);

} // resp3

// TODO: Move everything below to namespace resp3.

template <class T>
using basic_array_type = std::vector<T>;

/// RESP3 array type.
using array_type = basic_array_type<std::string>;

/// RESP3 map type.
using map_type = std::vector<std::string>;

/// RESP3 set type.
using set_type = std::vector<std::string>;

using number_type = long long int;
using bool_type = bool;
using double_type = std::string;
using blob_string_type = std::string;
using blob_error_type = std::string;
using simple_string_type = std::string;
using simple_error_type = std::string;
using big_number_type = std::string;
using verbatim_string_type = std::string;
using streamed_string_part_type = std::string;

struct transaction_element {
   int depth;
   resp3::type type;
   int expected_size = -1;
   command cmd = command::unknown;
   std::vector<std::string> value;
};

} // aedis

std::ostream& operator<<(std::ostream& os, aedis::resp3::type t);

