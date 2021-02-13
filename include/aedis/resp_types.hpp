/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vector>
#include <string>

namespace aedis { namespace resp {

template <class T, class Allocator = std::allocator<T>>
using basic_array_type = std::vector<T, Allocator>;

template <class T, class Allocator = std::allocator<T>>
using basic_map_type = std::vector<T, Allocator>;

template <class T, class Allocator = std::allocator<T>>
using basic_set_type = std::vector<T, Allocator>;

template<
   class CharT,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
using basic_streamed_string_part = std::basic_string<CharT, Traits, Allocator>;

template<
   class CharT,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
using basic_verbatim_string = std::basic_string<CharT, Traits, Allocator>;

template<
   class CharT,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
using basic_big_number = std::basic_string<CharT, Traits, Allocator>;

template<
   class CharT,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
using basic_blob_string = std::basic_string<CharT, Traits, Allocator>;

template<
   class CharT,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
using basic_blob_error = std::basic_string<CharT, Traits, Allocator>;

template<
   class CharT,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
using basic_simple_string = std::basic_string<CharT, Traits, Allocator>;

template<
   class CharT,
   class Traits = std::char_traits<CharT>,
   class Allocator = std::allocator<CharT>>
using basic_simple_error = std::basic_string<CharT, Traits, Allocator>;

using array_type = basic_array_type<std::string>;
using map_type = basic_map_type<std::string>;
using set_type = basic_set_type<std::string>;

using number_type = long long int;
using bool_type = bool;
using double_type = double;
using blob_string_type = basic_blob_string<char>;
using blob_error_type = basic_blob_error<char>;
using simple_string_type = basic_simple_string<char>;
using simple_error_type = basic_simple_error<char>;
using big_number_type = basic_big_number<char>;
using verbatim_string_type = basic_verbatim_string<char>;
using streamed_string_part_type = basic_streamed_string_part<char>;

} // resp
} // aedis
