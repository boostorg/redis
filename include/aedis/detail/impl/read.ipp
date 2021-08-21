/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/detail/read.hpp>

#define EXPAND_RECEIVER_CASE(type, cmd) case command::cmd: recv.on_##cmd(type); break

namespace aedis { namespace detail {

void
forward(
   command cmd,
   resp3::type type,
   receiver_base& recv)
{
   switch (cmd) {
      EXPAND_RECEIVER_CASE(type, acl_cat);
      EXPAND_RECEIVER_CASE(type, acl_deluser);
      EXPAND_RECEIVER_CASE(type, acl_genpass);
      EXPAND_RECEIVER_CASE(type, acl_getuser);
      EXPAND_RECEIVER_CASE(type, acl_help);
      EXPAND_RECEIVER_CASE(type, acl_list);
      EXPAND_RECEIVER_CASE(type, acl_load);
      EXPAND_RECEIVER_CASE(type, acl_log);
      EXPAND_RECEIVER_CASE(type, acl_save);
      EXPAND_RECEIVER_CASE(type, acl_setuser);
      EXPAND_RECEIVER_CASE(type, acl_users);
      EXPAND_RECEIVER_CASE(type, acl_whoami);
      EXPAND_RECEIVER_CASE(type, append);
      EXPAND_RECEIVER_CASE(type, del);
      EXPAND_RECEIVER_CASE(type, exec);
      EXPAND_RECEIVER_CASE(type, expire);
      EXPAND_RECEIVER_CASE(type, flushall);
      EXPAND_RECEIVER_CASE(type, get);
      EXPAND_RECEIVER_CASE(type, hdel);
      EXPAND_RECEIVER_CASE(type, hello);
      EXPAND_RECEIVER_CASE(type, hget);
      EXPAND_RECEIVER_CASE(type, hgetall);
      EXPAND_RECEIVER_CASE(type, hincrby);
      EXPAND_RECEIVER_CASE(type, hset);
      EXPAND_RECEIVER_CASE(type, hvals);
      EXPAND_RECEIVER_CASE(type, incr);
      EXPAND_RECEIVER_CASE(type, llen);
      EXPAND_RECEIVER_CASE(type, lpop);
      EXPAND_RECEIVER_CASE(type, lrange);
      EXPAND_RECEIVER_CASE(type, ltrim);
      EXPAND_RECEIVER_CASE(type, multi);
      EXPAND_RECEIVER_CASE(type, ping);
      EXPAND_RECEIVER_CASE(type, publish);
      EXPAND_RECEIVER_CASE(type, quit);
      EXPAND_RECEIVER_CASE(type, rpush);
      EXPAND_RECEIVER_CASE(type, sadd);
      EXPAND_RECEIVER_CASE(type, set);
      EXPAND_RECEIVER_CASE(type, smembers);
      EXPAND_RECEIVER_CASE(type, zadd);
      EXPAND_RECEIVER_CASE(type, zrange);
      EXPAND_RECEIVER_CASE(type, zrangebyscore);
      EXPAND_RECEIVER_CASE(type, zremrangebyscore);
      default: {assert(false);}
   }
}

response_adapter_base* select_buffer(response_adapters& adapters, resp3::type type, command cmd)
{
   if (type == resp3::type::push)
     return &adapters.resp_push;

   if (cmd == command::exec)
     return &adapters.resp_transaction;

   switch (type) {
      case resp3::type::set: return &adapters.resp_set;
      case resp3::type::map: return &adapters.resp_map;
      case resp3::type::attribute: return &adapters.resp_attribute;
      case resp3::type::array: return &adapters.resp_array;
      case resp3::type::simple_error: return &adapters.resp_simple_error;
      case resp3::type::simple_string: return &adapters.resp_simple_string;
      case resp3::type::number: return &adapters.resp_number;
      case resp3::type::doublean: return &adapters.resp_double;
      case resp3::type::big_number: return &adapters.resp_big_number;
      case resp3::type::boolean: return &adapters.resp_boolean;
      case resp3::type::blob_error: return &adapters.resp_blob_error;
      case resp3::type::blob_string: return &adapters.resp_blob_string;
      case resp3::type::verbatim_string: return &adapters.resp_verbatim_string;
      case resp3::type::streamed_string_part: return &adapters.resp_streamed_string_part;
      case resp3::type::null: return &adapters.resp_ignore;
      default: {
	 throw std::runtime_error("response_buffers");
	 return nullptr;
      }
   }
}

} // detail
} // aedis
