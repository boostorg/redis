/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_SENTINEL_COMMAND_HPP
#define AEDIS_SENTINEL_COMMAND_HPP

#include <ostream>

namespace aedis {
namespace sentinel {

/** \brief Sentinel commands.
 *  \ingroup any
 *
 *  The full list of Sentinel commands can be found at
 *  https://redis.io/topics/sentinel.
 *
 *  \remark This list was created with the help of the \c command
 *  command.
 */
enum class command {
   /// https://redis.io/commands/acl
   acl,
   /// https://redis.io/commands/auth
   auth,
   /// https://redis.io/commands/client
   client,
   /// https://redis.io/commands/command
   command,
   /// https://redis.io/commands/hello
   hello,
   /// https://redis.io/commands/info
   info,
   /// https://redis.io/commands/ping
   ping,
   /// https://redis.io/commands/psubscribe
   psubscribe,
   /// https://redis.io/commands/publish
   publish,
   /// https://redis.io/commands/punsubscribe
   punsubscribe,
   /// https://redis.io/commands/role
   role,
   /// https://redis.io/topics/sentinel
   sentinel,
   /// https://redis.io/commands/shutdown
   shutdown,
   /// https://redis.io/commands/subscribe
   subscribe,
   /// https://redis.io/commands/unsubscribe
   unsubscribe,
   /// Unknown/invalid command.
   invalid,
};

/** \brief Converts the command to a string.
 *  \ingroup any
 *  \param c The command to convert.
 */
char const* to_string(command c);

/** \brief Writes the command string to the stream.
 *  \ingroup any
 *  \param os Output stream.
 *  \param c Sentinel command
 */
std::ostream& operator<<(std::ostream& os, command c);

/** \brief Checks whether a command has push response.
 *  \ingroup any
 */
bool has_push_response(command cmd);

} // sentinel
} // aedis

#endif // AEDIS_SENTINEL_COMMAND_HPP
