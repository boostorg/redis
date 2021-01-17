/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

namespace aedis { namespace resp {

enum class command
{ append
, auth
, bgrewriteaof
, bgsave
, bitcount
, client
, del
, exec
, expire
, flushall
, get // 10
, hello
, hget
, hgetall
, hincrby
, hkeys
, hlen
, hmget
, hset
, hvals
, incr // 20
, keys
, llen
, lpop
, lpush
, lrange
, ltrim
, multi
, ping
, psubscribe
, publish // 30
, quit
, role
, rpush
, sadd
, scard
, sentinel
, set
, smembers
, subscribe
, unsubscribe // 40
, zadd
, zrange
, zrangebyscore
, zremrangebyscore
, none
};

#define EXPAND_COMMAND_CASE(x) case command::x: return #x

inline
auto const* to_string(command c)
{
   switch (c) {
      EXPAND_COMMAND_CASE(append);
      EXPAND_COMMAND_CASE(auth);
      EXPAND_COMMAND_CASE(bgrewriteaof);
      EXPAND_COMMAND_CASE(bgsave);
      EXPAND_COMMAND_CASE(bitcount);
      EXPAND_COMMAND_CASE(client);
      EXPAND_COMMAND_CASE(del);
      EXPAND_COMMAND_CASE(exec);
      EXPAND_COMMAND_CASE(expire);
      EXPAND_COMMAND_CASE(flushall);
      EXPAND_COMMAND_CASE(get);
      EXPAND_COMMAND_CASE(hello);
      EXPAND_COMMAND_CASE(hget);
      EXPAND_COMMAND_CASE(hgetall);
      EXPAND_COMMAND_CASE(hincrby);
      EXPAND_COMMAND_CASE(hkeys);
      EXPAND_COMMAND_CASE(hlen);
      EXPAND_COMMAND_CASE(hmget);
      EXPAND_COMMAND_CASE(hset);
      EXPAND_COMMAND_CASE(hvals);
      EXPAND_COMMAND_CASE(incr);
      EXPAND_COMMAND_CASE(keys);
      EXPAND_COMMAND_CASE(llen);
      EXPAND_COMMAND_CASE(lpop);
      EXPAND_COMMAND_CASE(lpush);
      EXPAND_COMMAND_CASE(lrange);
      EXPAND_COMMAND_CASE(ltrim);
      EXPAND_COMMAND_CASE(multi);
      EXPAND_COMMAND_CASE(ping);
      EXPAND_COMMAND_CASE(psubscribe);
      EXPAND_COMMAND_CASE(publish);
      EXPAND_COMMAND_CASE(quit);
      EXPAND_COMMAND_CASE(role);
      EXPAND_COMMAND_CASE(rpush);
      EXPAND_COMMAND_CASE(sadd);
      EXPAND_COMMAND_CASE(scard);
      EXPAND_COMMAND_CASE(sentinel);
      EXPAND_COMMAND_CASE(set);
      EXPAND_COMMAND_CASE(smembers);
      EXPAND_COMMAND_CASE(subscribe);
      EXPAND_COMMAND_CASE(unsubscribe);
      EXPAND_COMMAND_CASE(zadd);
      EXPAND_COMMAND_CASE(zrange);
      EXPAND_COMMAND_CASE(zrangebyscore);
      EXPAND_COMMAND_CASE(zremrangebyscore);
      EXPAND_COMMAND_CASE(none);
      default: assert(false);
   }
}

}
}

