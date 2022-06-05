/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_ERROR_HPP
#define AEDIS_ERROR_HPP

#include <boost/system/error_code.hpp>

namespace aedis {

/** \brief Generic errors.
 *  \ingroup any
 */
enum class error
{
   /// Represents the timeout of the resolve operation.
   resolve_timeout = 1,

   /// Represents the timeout of the connect operation.
   connect_timeout,

   /// Represents the timeout of the read operation.
   read_timeout,

   /// Represents the timeout of the write operation.
   write_timeout,

   /// Idle timeout.
   idle_timeout,
};

/** \brief Creates a error_code object from an error.
 *  \ingroup any
 */
boost::system::error_code make_error_code(error e);

} // aedis

namespace std {

template<>
struct is_error_code_enum<::aedis::error> : std::true_type {};

} // std

#endif // AEDIS_ERROR_HPP
