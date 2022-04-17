/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <boost/system/error_code.hpp>

namespace aedis {
namespace generic {

/** \brief Errors from generic module
 *  \ingroup any
 */
enum class error
{
   /// Read operation has timed out.
   read_timeout = 1,

   /// Write operation has timed out.
   write_timeout,

   /// Write operation has timed out.
   connect_timeout,
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
