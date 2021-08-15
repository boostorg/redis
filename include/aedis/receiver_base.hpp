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

#define RECEIVER_FUNCTION_REF(name, cmd) virtual void on_##cmd(name& v) noexcept { }
#define RECEIVER_FUNCTION(name, cmd) virtual void on_##cmd(name v) noexcept { }

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

   /// Receiver of an acl_list command.
   RECEIVER_FUNCTION_REF(array_type, acl_list);

   /// Receiver of an acl_users command.
   RECEIVER_FUNCTION_REF(array_type, acl_users);

   /// Receiver of an acl_getuser command.
   RECEIVER_FUNCTION_REF(array_type, acl_getuser);

   /// Receiver of an acl_cat command.
   RECEIVER_FUNCTION_REF(array_type, acl_cat);

   /// Receiver of an acl_log command.
   RECEIVER_FUNCTION_REF(array_type, acl_log);

   /// Receiver of an acl_help command.
   RECEIVER_FUNCTION_REF(array_type, acl_help);

   /// Receiver of an lrange command.
   RECEIVER_FUNCTION_REF(array_type, lrange);

   /// Receiver of an lpop command.
   RECEIVER_FUNCTION_REF(array_type, lpop);

   /// Receiver of an hgetall command.
   RECEIVER_FUNCTION_REF(array_type, hgetall);

   /// Receiver of an hvals command.
   RECEIVER_FUNCTION_REF(array_type, hvals);

   /// Receiver of an zrange command.
   RECEIVER_FUNCTION_REF(array_type, zrange);

   /// Receiver of an zrangebyscore command.
   RECEIVER_FUNCTION_REF(array_type, zrangebyscore);


   /// Receiver of an hello command.
   RECEIVER_FUNCTION_REF(map_type, hello);


   /// Receiver of an smembers command.
   RECEIVER_FUNCTION_REF(set_type, smembers);


   /// Receiver of an acl_load command.
   RECEIVER_FUNCTION_REF(simple_string_type, acl_load);

   /// Receiver of an acl_save command.
   RECEIVER_FUNCTION_REF(simple_string_type, acl_save);

   /// Receiver of an acl_setuser command.
   RECEIVER_FUNCTION_REF(simple_string_type, acl_setuser);

   /// Receiver of an acl_log command.
   RECEIVER_FUNCTION_REF(simple_string_type, acl_log);

   /// Receiver of an acl_ping command.
   RECEIVER_FUNCTION_REF(simple_string_type, ping);

   /// Receiver of an quit command.
   RECEIVER_FUNCTION_REF(simple_string_type, quit);

   /// Receiver of an flushall command.
   RECEIVER_FUNCTION_REF(simple_string_type, flushall);

   /// Receiver of an ltrim command.
   RECEIVER_FUNCTION_REF(simple_string_type, ltrim);

   /// Receiver of an set command.
   RECEIVER_FUNCTION_REF(simple_string_type, set);


   /// Receiver of an acl_deluser command.
   RECEIVER_FUNCTION(number_type, acl_deluser);

   /// Receiver of an rpush command.
   RECEIVER_FUNCTION(number_type, rpush);

   /// Receiver of an del command.
   RECEIVER_FUNCTION(number_type, del);

   /// Receiver of an llen command.
   RECEIVER_FUNCTION(number_type, llen);

   /// Receiver of an publish command.
   RECEIVER_FUNCTION(number_type, publish);

   /// Receiver of an incr command.
   RECEIVER_FUNCTION(number_type, incr);

   /// Receiver of an append command.
   RECEIVER_FUNCTION(number_type, append);

   /// Receiver of an hset command.
   RECEIVER_FUNCTION(number_type, hset);

   /// Receiver of an hincrby command.
   RECEIVER_FUNCTION(number_type, hincrby);

   /// Receiver of an zadd command.
   RECEIVER_FUNCTION(number_type, zadd);

   /// Receiver of an zremrangebyscore command.
   RECEIVER_FUNCTION(number_type, zremrangebyscore);

   /// Receiver of an expire command.
   RECEIVER_FUNCTION(number_type, expire);

   /// Receiver of an sadd command.
   RECEIVER_FUNCTION(number_type, sadd);

   /// Receiver of an hdel command.
   RECEIVER_FUNCTION(number_type, hdel);


   /// Receiver of an acl_genpass command.
   RECEIVER_FUNCTION_REF(blob_string_type, acl_genpass);

   /// Receiver of an acl_whoami command.
   RECEIVER_FUNCTION_REF(blob_string_type, acl_whoami);

   /// Receiver of an lpop command.
   RECEIVER_FUNCTION_REF(blob_string_type, lpop);

   /// Receiver of an get command.
   RECEIVER_FUNCTION_REF(blob_string_type, get);

   /// Receiver of an hget command.
   RECEIVER_FUNCTION_REF(blob_string_type, hget);

   /// Callback for push notifications
   virtual void on_push(array_type& v) noexcept { }

   /// Callback for simple error.
   virtual void on_simple_error(command cmd, simple_error_type& v) noexcept { }

   /// Callback for blob error.
   virtual void on_blob_error(command cmd, blob_error_type& v) noexcept { }

   /// Callback from null responses.
   virtual void on_null(command cmd) noexcept { }

   /// Receives a transaction
   virtual void
   on_transaction(std::vector<transaction_element>& result) noexcept { }
};

} // aedis
