/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <ostream>

/** \file command.hpp
 *  \brief Enum of all redis commands.
 *
 *  This file contains the enum used to identify each redis command
 *  and some helper functions.
 */

namespace aedis {

/// List of the supported redis commands.
enum class command
{ /// https://redis.io/commands/acl-load
  acl_load,
  /// https://redis.io/commands/acl-save
  acl_save,
  /// https://redis.io/commands/acl-list
  acl_list,
  /// https://redis.io/commands/acl-users
  acl_users,
  /// https://redis.io/commands/acl-getuser
  acl_getuser,
  /// https://redis.io/commands/acl-setuser
  acl_setuser,
  /// https://redis.io/commands/acl-deluser
  acl_deluser,
  /// https://redis.io/commands/acl-cat
  acl_cat,
  /// https://redis.io/commands/acl-genpass
  acl_genpass,
  /// https://redis.io/commands/acl-whoami
  acl_whoami,
  /// https://redis.io/commands/acl-log
  acl_log,
  /// https://redis.io/commands/acl-help
  acl_help,
  /// https://redis.io/commands/append
  append,
  /// https://redis.io/commands/bgrewriteaof
  auth,
  /// https://redis.io/commands/bgrewriteaof
  bgrewriteaof,
  /// https://redis.io/commands/bgsave
  bgsave,
  /// https://redis.io/commands/bitcount
  bitcount,
  /// https://redis.io/commands/client_id
  client_id,
  /// https://redis.io/commands/del
  del,
  /// https://redis.io/commands/exec
  exec,
  /// https://redis.io/commands/expire
  expire,
  /// https://redis.io/commands/flushall
  flushall,
  /// https://redis.io/commands/get
  get,
  /// https://redis.io/commands/hello
  hello,
  /// https://redis.io/commands/hget
  hget,
  /// https://redis.io/commands/hgetall
  hgetall,
  /// https://redis.io/commands/hincrby
  hincrby,
  /// https://redis.io/commands/hkeys
  hkeys,
  /// https://redis.io/commands/hlen
  hlen,
  /// https://redis.io/commands/hmget
  hmget,
  /// https://redis.io/commands/hset
  hset,
  /// https://redis.io/commands/hvals
  hvals,
  /// https://redis.io/commands/hdel
  hdel,
  /// https://redis.io/commands/incr
  incr,
  /// https://redis.io/commands/keys
  keys,
  /// https://redis.io/commands/llen
  llen,
  /// https://redis.io/commands/lpop
  lpop,
  /// https://redis.io/commands/lpush
  lpush,
  /// https://redis.io/commands/lrange
  lrange,
  /// https://redis.io/commands/ltrim
  ltrim,
  /// https://redis.io/commands/multi
  multi,
  /// https://redis.io/commands/ping
  ping,
  /// https://redis.io/commands/psubscribe
  psubscribe,
  /// https://redis.io/commands/publish
  publish,
  /// https://redis.io/commands/quit
  quit,
  /// https://redis.io/commands/role
  role,
  /// https://redis.io/commands/rpush
  rpush,
  /// https://redis.io/commands/sadd
  sadd,
  /// https://redis.io/commands/scard
  scard,
  /// https://redis.io/commands/sdiff
  sdiff,
  /// https://redis.io/commands/sentinel
  sentinel,
  /// https://redis.io/commands/set
  set,
  /// https://redis.io/commands/smembers
  smembers,
  /// https://redis.io/commands/subscribe
  subscribe,
  /// https://redis.io/commands/unsubscribe
  unsubscribe,
  /// https://redis.io/commands/zadd
  zadd,
  /// https://redis.io/commands/zrange
  zrange,
  /// https://redis.io/commands/zrangebyscore
  zrangebyscore,
  /// https://redis.io/commands/zremrangebyscore
  zremrangebyscore,
  /// Invalid or unknown command.
  unknown,
};

/// Returns the string representation of a command.
char const* to_string(command c);

/// Writes the command to the output stream.
std::ostream& operator<<(std::ostream& os, command c);

} // aedis
