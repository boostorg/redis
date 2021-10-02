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

// FIXME: We shouldn't need this header here.
#include <aedis/command.hpp>

namespace aedis { namespace resp3 {

enum class type
{ array
, flat_array
, flat_push
, flat_set
, flat_map
, flat_attribute
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

template <class T>
using basic_flat_array = std::vector<T>;

/// RESP3 flat array types.
using flat_array_type = basic_flat_array<std::string>;
using flat_array_int_type = basic_flat_array<int>;

using flat_push_type = std::vector<std::string>;

/// RESP3 map type.
using flat_map_type = std::vector<std::string>;

/// RESP3 set type.
using flat_set_type = std::vector<std::string>;

using number_type = long long int;
using boolean_type = bool;
using doublean_type = std::string;
using blob_string_type = std::string;
using blob_error_type = std::string;
using simple_string_type = std::string;
using simple_error_type = std::string;
using big_number_type = std::string;
using verbatim_string_type = std::string;
using streamed_string_part_type = std::string;

struct node {
   struct description {
      std::size_t depth;
      type data_type;
   };

   std::vector<description> desc;
   std::vector<std::string> values;

   void clear()
   {
      desc.clear();
      values.clear();
   }
};

using array_type = node;

} // resp3
} // aedis

std::ostream& operator<<(std::ostream& os, aedis::resp3::type t);

