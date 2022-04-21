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
   /// Represents a timeout of the resolve operation.
   resolve_timeout = 1,

   /// Represents a timeout of the connect operation.
   connect_timeout,

   /// Represents a timeout of the read operation.
   read_timeout,

   /// Represents a timeout of the write operation.
   write_timeout,

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
