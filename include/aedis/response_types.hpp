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

using response_blob_string = response_basic_blob_string<char>;
using response_blob_error = response_basic_blob_error<char>;
using response_simple_string = response_basic_simple_string<char>;
using response_simple_error = response_basic_simple_error<char>;
using response_big_number = response_basic_big_number<char>;
using response_verbatim_string = response_basic_verbatim_string<char>;
using response_streamed_string_part = response_basic_streamed_string_part<char>;

} // resp
} // aedis

