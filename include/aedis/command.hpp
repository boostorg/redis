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
  /// Adds ping to the request, see https://redis.io/commands/append
, append
  /// Adds auth to the request, see https://redis.io/commands/bgrewriteaof
, auth
  /// Adds bgrewriteaof to the request, see https://redis.io/commands/bgrewriteaof
, bgrewriteaof
  /// Adds bgsave to the request, see //https://redis.io/commands/bgsave
, bgsave
, bitcount
  /// Adds ping to the request, see https://redis.io/commands/client_id
, client_id
  /// Adds ping to the request, see https://redis.io/commands/del
, del
, exec
, expire
  /// Adds ping to the request, see https://redis.io/commands/flushall
, flushall
  /// Adds ping to the request, see https://redis.io/commands/get
, get
  /// Adds ping to the request, see https://redis.io/commands/hello
, hello
, hget
  /// Adds ping to the request, see https://redis.io/commands/hgetall
, hgetall
, hincrby
  /// Adds ping to the request, see https://redis.io/commands/hkeys
, hkeys
  /// Adds ping to the request, see https://redis.io/commands/hlen
, hlen
, hmget
, hset
  /// Adds ping to the request, see https://redis.io/commands/hvals
, hvals
, hdel
, incr
  /// Adds ping to the request, see https://redis.io/commands/keys
, keys
  /// Adds ping to the request, see https://redis.io/commands/llen
, llen
  /// Adds ping to the request, see https://redis.io/commands/lpop
, lpop
, lpush
, lrange
, ltrim
, multi
, ping
, psubscribe
, publish
, quit
  /// Adds role to the request, see https://redis.io/commands/role
, role
, rpush
, sadd
  /// Adds ping to the request, see https://redis.io/commands/scard
, scard
, sdiff
, sentinel
, set
  /// Adds ping to the request, see https://redis.io/commands/smembers
, smembers
, subscribe
  /// Adds ping to the request, see https://redis.io/commands/unsubscribe
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

