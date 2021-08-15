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

type to_type(char c);

// TODO: Move everything below to namespace resp3.

template <class T>
using basic_array = std::vector<T>;

/// RESP3 array type.
using array = basic_array<std::string>;
using array_int = basic_array<int>;

/// RESP3 map type.
using map = std::vector<std::string>;

/// RESP3 set type.
using set = std::vector<std::string>;

using number = long long int;
using boolean = bool;
using doublean = std::string;
using blob_string = std::string;
using blob_error = std::string;
using simple_string = std::string;
using simple_error = std::string;
using big_number = std::string;
using verbatim_string = std::string;
using streamed_string_part = std::string;

struct transaction_item {
   int depth;
   resp3::type type;
   int expected_size = -1;
   command cmd = command::unknown;
   std::vector<std::string> value;
};

using transaction_result = std::vector<transaction_item>;

} // resp3
} // aedis

std::ostream& operator<<(std::ostream& os, aedis::resp3::type t);

