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
#include "utils.hpp"
#include "response.hpp"
#include "request.hpp"

namespace aedis {

#define RECEIVER_FUNCTION_REF(name, cmd) virtual void on_##cmd(resp::name& v) noexcept { }
#define RECEIVER_FUNCTION(name, cmd) virtual void on_##cmd(resp::name v) noexcept { }

class receiver_base {
public:
   RECEIVER_FUNCTION_REF(array_type, acl_list);
   RECEIVER_FUNCTION_REF(array_type, acl_users);
   RECEIVER_FUNCTION_REF(array_type, acl_getuser);
   RECEIVER_FUNCTION_REF(array_type, acl_cat);
   RECEIVER_FUNCTION_REF(array_type, acl_log);
   RECEIVER_FUNCTION_REF(array_type, acl_help);
   RECEIVER_FUNCTION_REF(array_type, lrange);
   RECEIVER_FUNCTION_REF(array_type, lpop);
   RECEIVER_FUNCTION_REF(array_type, hgetall);
   RECEIVER_FUNCTION_REF(array_type, zrange);
   RECEIVER_FUNCTION_REF(array_type, zrangebyscore);

   RECEIVER_FUNCTION_REF(map_type, hello);

   RECEIVER_FUNCTION_REF(simple_string_type, acl_load);
   RECEIVER_FUNCTION_REF(simple_string_type, acl_save);
   RECEIVER_FUNCTION_REF(simple_string_type, acl_setuser);
   RECEIVER_FUNCTION_REF(simple_string_type, acl_log);
   RECEIVER_FUNCTION_REF(simple_string_type, ping);
   RECEIVER_FUNCTION_REF(simple_string_type, quit);
   RECEIVER_FUNCTION_REF(simple_string_type, flushall);
   RECEIVER_FUNCTION_REF(simple_string_type, ltrim);
   RECEIVER_FUNCTION_REF(simple_string_type, set);

   RECEIVER_FUNCTION(number_type, acl_deluser);
   RECEIVER_FUNCTION(number_type, rpush);
   RECEIVER_FUNCTION(number_type, del);
   RECEIVER_FUNCTION(number_type, llen);
   RECEIVER_FUNCTION(number_type, publish);
   RECEIVER_FUNCTION(number_type, incr);
   RECEIVER_FUNCTION(number_type, append);
   RECEIVER_FUNCTION(number_type, hset);
   RECEIVER_FUNCTION(number_type, hincrby);
   RECEIVER_FUNCTION(number_type, zadd);
   RECEIVER_FUNCTION(number_type, zremrangebyscore);
   RECEIVER_FUNCTION(number_type, expire);

   RECEIVER_FUNCTION_REF(blob_string_type, acl_genpass);
   RECEIVER_FUNCTION_REF(blob_string_type, acl_whoami);
   RECEIVER_FUNCTION_REF(blob_string_type, lpop);
   RECEIVER_FUNCTION_REF(blob_string_type, get);
   RECEIVER_FUNCTION_REF(blob_string_type, hget);

   virtual void on_push(resp::array_type& v) noexcept { }
   virtual void on_simple_error(command cmd, resp::simple_error_type& v) noexcept { }
   virtual void on_blob_error(command cmd, resp::blob_error_type& v) noexcept { }
   virtual void on_null(command cmd) noexcept { }
};

} // aedis
