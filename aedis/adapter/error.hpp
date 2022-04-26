/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_ADAPTER_ERROR_HPP
#define AEDIS_ADAPTER_ERROR_HPP

#include <system_error>

namespace aedis {
namespace adapter {

/** \brief Adapter errors.
 *  \ingroup any
 */
enum class error
{
   /// Expects a simple RESP3 type but got an aggregate.
   expects_simple_type = 1,

   /// Expects aggregate type.
   expects_aggregate_type,

   /// Expects a map but got other aggregate.
   expects_map_type,

   /// Expects a set aggregate but got something else.
   expects_set_type,

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

/** \brief Creates a error_code object from an error.
 *  \ingroup any
 */
boost::system::error_code make_error_code(error e);

} // adapter
} // aedis

namespace std {

template<>
struct is_error_code_enum<::aedis::adapter::error> : std::true_type {};

} // std

#endif // AEDIS_ADAPTER_ERROR_HPP
