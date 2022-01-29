/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <ostream>

namespace aedis {
namespace sentinel {

/** \brief Sentinel commands.
 *  \ingroup enums
 *
 *  For a full list of commands see https://redis.io/topics/sentinel
 *
 *  \remark The list of commands below are read from Redis with the
 *  help of the command \c command.
 */
enum class command {
   acl,
   auth,
   client,
   command,
   hello,
   info,
   ping,
   psubscribe,
   publish,
   punsubscribe,
   role,
   sentinel,
   shutdown,
   subscribe,
   unsubscribe,
   unknown
};

/** \brief Converts a sentinel command to a string
 *  \ingroup functions
 *
 *  \param c The command to convert.
 */
char const* to_string(command c);

/** \brief Write the text for a sentinel command name to an output stream.
 *  \ingroup operators
 *
 *  \param os Output stream.
 *  \param c Sentinel command
 */
std::ostream& operator<<(std::ostream& os, command c);

/** \brief Returns true for sentinel commands with push response.
 *  \ingroup functions
 */
bool has_push_response(command cmd);

} // sentinel
} // aedis
