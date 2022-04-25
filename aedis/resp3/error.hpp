/* Copyright (c) 2018 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_RESP3_ERROR_HPP
#define AEDIS_RESP3_ERROR_HPP

#include <boost/system/error_code.hpp>

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
   unexpected_bool_value,

   /// Expected field value is empty.
   empty_field
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

#endif // AEDIS_RESP3_ERROR_HPP
