/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <system_error>

namespace aedis {
namespace resp3 {

/** \brief RESP3 parsing errors.
 *  \ingroup any
 */
enum class error
{
   /// Invalid RESP3 type.
   invalid_type = 1,

   /// Can't parse the string as an integer.
   not_a_number,

   /// Received less bytes than expected.
   unexpected_read_size,

   /// The maximum depth of a nested response was exceeded.
   exceeeds_max_nested_depth,

   /// Unexpected bool value
   unexpected_bool_value
};

/** \brief Converts an error in an boost::system::error_code object.
 *  \ingroup any
 */
boost::system::error_code make_error_code(error e);

/** \brief todo
 *  \ingroup any
 */
boost::system::error_condition make_error_condition(error e);

} // resp3
} // aedis

namespace std {

template<>
struct is_error_code_enum<::aedis::resp3::error> : std::true_type {};

} // std
