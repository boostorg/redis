/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_OPERATION_HPP
#define AEDIS_OPERATION_HPP

namespace aedis {

/** \brief Connection operations that can be cancelled.
 *  \ingroup high-level-api
 *  
 *  The operations listed below can be passed to the
 *  `aedis::connection::cancel` member function.
 */
enum class operation {
   /// Refers to `connection::async_exec` operations.
   exec,
   /// Refers to `connection::async_run` operations.
   run,
   /// Refers to `connection::async_receive` operations.
   receive,
};

} // aedis

#endif // AEDIS_OPERATION_HPP
