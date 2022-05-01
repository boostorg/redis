/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_RESP3_ERROR_HPP
#define AEDIS_RESP3_ERROR_HPP

#include <boost/system/error_code.hpp>

namespace aedis {
namespace resp3 {

/** \brief RESP3 errors.
 *  \ingroup any
 */
enum class error
{
   /// Invalid RESP3 type.
   invalid_type = 1,

   /// Can't parse the string as a number.
   not_a_number,

   /// Received less bytes than expected.
   unexpected_read_size,

   /// The maximum depth of a nested response was exceeded.
   exceeeds_max_nested_depth,

   /// Unexpects bool value.
   unexpected_bool_value,

   /// Expected field value is empty.
   empty_field
};

/** \brief Creates a error_code object from an error.
 *  \ingroup any
 */
boost::system::error_code make_error_code(error e);

} // resp3
} // aedis

namespace std {

template<>
struct is_error_code_enum<::aedis::resp3::error> : std::true_type {};

} // std

#endif // AEDIS_RESP3_ERROR_HPP
