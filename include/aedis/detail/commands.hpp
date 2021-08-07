/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <ostream>

namespace aedis {

enum class commands
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
, append
, auth
, bgrewriteaof
, bgsave
, bitcount
, client_id
, del
, exec
, expire
, flushall
, get
, hello
, hget
, hgetall
, hincrby
, hkeys
, hlen
, hmget
, hset
, hvals
, hdel
, incr
, keys
, llen
, lpop
, lpush
, lrange
, ltrim
, multi
, ping
, psubscribe
, publish
, quit
, role
, rpush
, sadd
, scard
, sentinel
, set
, smembers
, subscribe
, unsubscribe
, zadd
, zrange
, zrangebyscore
, zremrangebyscore
, none
};

std::string to_string(commands c);
std::ostream& operator<<(std::ostream& os, commands c);

} // aedis

