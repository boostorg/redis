/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/command.hpp>

#include <cassert>

namespace aedis {

char const* to_string(command c)
{
   static char const* table[] =
   { "ACL LOAD"
   , "ACL SAVE"
   , "ACL LIST"
   , "ACL USERS"
   , "ACL GETUSER"
   , "ACL SETUSER"
   , "ACL DELUSER"
   , "ACL CAT"
   , "ACL GENPASS"
   , "ACL WHOAMI"
   , "ACL LOG"
   , "ACL HELP"
   , "APPEND"
   , "AUTH"
   , "BGREWRITEAOF"
   , "BGSAVE"
   , "BITCOUNT"
   , "CLIENT_ID"
   , "DEL"
   , "EXEC"
   , "EXPIRE"
   , "FLUSHALL"
   , "GET"
   , "HELLO"
   , "HGET"
   , "HGETALL"
   , "HINCRBY"
   , "HKEYS"
   , "HLEN"
   , "HMGET"
   , "HSET"
   , "HVALS"
   , "HDEL"
   , "INCR"
   , "KEYS"
   , "LLEN"
   , "LPOP"
   , "LPUSH"
   , "LRANGE"
   , "LTRIM"
   , "MULTI"
   , "PING"
   , "PSUBSCRIBE"
   , "PUBLISH"
   , "QUIT"
   , "ROLE"
   , "RPUSH"
   , "SADD"
   , "SCARD"
   , "SDIFF"
   , "SENTINEL"
   , "SET"
   , "SMEMBERS"
   , "SUBSCRIBE"
   , "UNSUBSCRIBE"
   , "ZADD"
   , "ZRANGE"
   , "ZRANGEBYSCORE"
   , "ZREMRANGEBYSCORE"
   , "UNKNOWN"
   };

   return table[static_cast<int>(c)];
}

std::ostream& operator<<(std::ostream& os, command c)
{
   os << to_string(c);
   return os;
}

} // aedis
