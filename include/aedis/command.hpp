/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <ostream>

namespace aedis {

/// List of the supported redis commands.
enum class command
{ acl_load
, acl_save
, acl_list
, acl_users
, acl_getuser
, acl_setuser
, acl_deluser
, acl_cat
, acl_genpass
, acl_whoami
, acl_log
, acl_help
  /// https://redis.io/commands/append
, append
  /// https://redis.io/commands/bgrewriteaof
, auth
  /// https://redis.io/commands/bgrewriteaof
, bgrewriteaof
  /// https://redis.io/commands/bgsave
, bgsave
, bitcount
  /// https://redis.io/commands/client_id
, client_id
  /// https://redis.io/commands/del
, del
, exec
, expire
  /// https://redis.io/commands/flushall
, flushall
  /// https://redis.io/commands/get
, get
  /// https://redis.io/commands/hello
, hello
, hget
  /// https://redis.io/commands/hgetall
, hgetall
, hincrby
  /// https://redis.io/commands/hkeys
, hkeys
  /// https://redis.io/commands/hlen
, hlen
, hmget
, hset
  /// https://redis.io/commands/hvals
, hvals
, hdel
, incr
  /// https://redis.io/commands/keys
, keys
  /// https://redis.io/commands/llen
, llen
  /// https://redis.io/commands/lpop
, lpop
, lpush
, lrange
, ltrim
, multi
, ping
, psubscribe
  /// https://redis.io/commands/publish
, publish
, quit
  /// https://redis.io/commands/role
, role
, rpush
, sadd
  /// https://redis.io/commands/scard
, scard
, sdiff
  /// https://redis.io/commands/sentinel
, sentinel
, set
  /// https://redis.io/commands/smembers
, smembers
, subscribe
  /// https://redis.io/commands/unsubscribe
, unsubscribe
, zadd
, zrange
, zrangebyscore
, zremrangebyscore
, unknown
};

/// Converts the command to a string.
char const* as_string(command c);

/** Writes the text representation of the command to the output
 *  stream.
 */
std::ostream& operator<<(std::ostream& os, command c);

} // aedis

