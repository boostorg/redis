/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/detail/responses.hpp>

namespace aedis { namespace detail {

using response_array = detail::response_basic_array<std::string>;
using response_map = detail::response_basic_map<std::string>;
using response_set = detail::response_basic_set<std::string>;

using response_blob_string = detail::response_basic_blob_string<char>;
using response_blob_error = detail::response_basic_blob_error<char>;
using response_simple_string = detail::response_basic_simple_string<char>;
using response_simple_error = detail::response_basic_simple_error<char>;
using response_big_number = detail::response_basic_big_number<char>;
using response_verbatim_string = detail::response_basic_verbatim_string<char>;
using response_streamed_string_part = detail::response_basic_streamed_string_part<char>;

} // detail
} // aedis

