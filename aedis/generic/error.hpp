/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_GENERIC_ERROR_HPP
#define AEDIS_GENERIC_ERROR_HPP

#include <boost/system/error_code.hpp>

namespace aedis {
namespace generic {

/** \brief Errors from generic module
 *  \ingroup any
 */
enum class error
{
   /// Represents a timeout of the resolve operation.
   resolve_timeout = 1,

   /// Represents a timeout of the connect operation.
   connect_timeout,

   /// Represents a timeout of the read operation.
   read_timeout,

   /// Represents a timeout of the write operation.
   write_timeout,

   /// Idle timeout.
   idle_timeout,

   /// Write stop requested.
   write_stop_requested,
};

/** \brief Converts an error in an boost::system::error_code object.
 *  \ingroup any
 */
boost::system::error_code make_error_code(error e);

/** \brief Make an error condition.
 *  \ingroup any
 */
boost::system::error_condition make_error_condition(error e);

} // generic
} // aedis

namespace std {

template<>
struct is_error_code_enum<::aedis::generic::error> : std::true_type {};

} // std

#endif // AEDIS_GENERIC_ERROR_HPP
