/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <memory>
#include <iostream>

#include "type.hpp"
#include "pipeline.hpp"

namespace aedis {

#define RECEIVER_FUNCTION(cmd) virtual void on_##cmd(resp3::type type) noexcept { }

/** Receiver base class.
 *
 *  This is a class users should derive from in order to receive
 *  response to redis commands, see the \ref request class for how to
 *  compose redis commands in a pipeline.
 *
 *  The RESP3 data types suported can be found on
 *  https://github.com/antirez/RESP3/blob/74adea588783e463c7e84793b325b088fe6edd1c/spec.md#resp3-types
 */

class receiver_base {
public:
   RECEIVER_FUNCTION(acl_cat);
   RECEIVER_FUNCTION(acl_deluser);
   RECEIVER_FUNCTION(acl_genpass);
   RECEIVER_FUNCTION(acl_getuser);
   RECEIVER_FUNCTION(acl_help);
   RECEIVER_FUNCTION(acl_list);
   RECEIVER_FUNCTION(acl_load);
   RECEIVER_FUNCTION(acl_log);
   RECEIVER_FUNCTION(acl_save);
   RECEIVER_FUNCTION(acl_setuser);
   RECEIVER_FUNCTION(acl_users);
   RECEIVER_FUNCTION(acl_whoami);
   RECEIVER_FUNCTION(append);
   RECEIVER_FUNCTION(del);
   RECEIVER_FUNCTION(exec);
   RECEIVER_FUNCTION(expire);
   RECEIVER_FUNCTION(flushall);
   RECEIVER_FUNCTION(get);
   RECEIVER_FUNCTION(hdel);
   RECEIVER_FUNCTION(hello);
   RECEIVER_FUNCTION(hget);
   RECEIVER_FUNCTION(hgetall);
   RECEIVER_FUNCTION(hincrby);
   RECEIVER_FUNCTION(hset);
   RECEIVER_FUNCTION(hvals);
   RECEIVER_FUNCTION(incr);
   RECEIVER_FUNCTION(llen);
   RECEIVER_FUNCTION(lpop);
   RECEIVER_FUNCTION(lrange);
   RECEIVER_FUNCTION(ltrim);
   RECEIVER_FUNCTION(multi);
   RECEIVER_FUNCTION(ping);
   RECEIVER_FUNCTION(publish);
   RECEIVER_FUNCTION(quit);
   RECEIVER_FUNCTION(rpush);
   RECEIVER_FUNCTION(sadd);
   RECEIVER_FUNCTION(set);
   RECEIVER_FUNCTION(smembers);
   RECEIVER_FUNCTION(zadd);
   RECEIVER_FUNCTION(zrange);
   RECEIVER_FUNCTION(zrangebyscore);
   RECEIVER_FUNCTION(zremrangebyscore);
   virtual void on_push() noexcept { }
};

} // aedis
