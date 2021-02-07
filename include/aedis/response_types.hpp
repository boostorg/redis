/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "response.hpp"

namespace aedis { namespace resp {

using response_array = response_basic_array<std::string>;
using response_map = response_basic_map<std::string>;
using response_set = response_basic_set<std::string>;

using response_number = response_basic_number<long long int>;
using response_blob_string = response_basic_blob_string<char>;
using response_blob_error = response_basic_blob_error<char>;
using response_simple_string = response_basic_simple_string<char>;
using response_simple_error = response_basic_simple_error<char>;
using response_big_number = response_basic_big_number<char>;
using response_double = response_basic_double<char>;
using response_verbatim_string = response_basic_verbatim_string<char>;
using response_streamed_string_part = response_basic_streamed_string_part<char>;


using array_type = response_array::data_type;
using map_type = response_map::data_type;
using set_type = response_set::data_type;

using blob_string_type = response_blob_string::data_type;
using simple_string_type = response_simple_string::data_type;
using big_number_type = response_big_number::data_type;
using double_type = response_double::data_type;
using verbatim_string_type = response_verbatim_string::data_type;
using streamed_string_part_type = response_streamed_string_part::data_type;
using bool_type = response_bool::data_type;

} // resp
} // aedis

