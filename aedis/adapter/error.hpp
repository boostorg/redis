/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <system_error>

namespace aedis {
namespace adapter {

/** \brief Errors that may occurr when reading a response.
 *  \ingroup any
 */
enum class error
{
   /// Expects a simple RESP3 type but got an aggregate.
   expects_simple_type = 1,

   /// Expects aggregate type.
   expects_aggregate,

   /// Expects a map but got other aggregate.
   expects_map_like_aggregate,

   /// Expects a set aggregate but got something else.
   expects_set_aggregate,

   /// Nested response not supported.
   nested_aggregate_unsupported,

   /// Got RESP3 simple error.
   simple_error,

   /// Got RESP3 blob_error.
   blob_error,

   /// Aggregate container has incompatible size.
   incompatible_size,

   /// Not a double
   not_a_double,

   /// Got RESP3 null type.
   null
};

/** \brief todo
 *  \ingroup any
 */
boost::system::error_code make_error_code(error e);

/** \brief todo
 *  \ingroup any
 */
boost::system::error_condition make_error_condition(error e);

} // adapter
} // aedis

namespace std {

template<>
struct is_error_code_enum<::aedis::adapter::error> : std::true_type {};

} // std
