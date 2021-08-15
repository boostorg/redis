/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/detail/read.hpp>

#define EXPAND_RECEIVER_CASE(var, x) case command::x: recv.on_##x(*var.result); break

namespace aedis { namespace detail {

bool queue_pop(std::queue<pipeline>& reqs)
{
   assert(!std::empty(reqs));
   assert(!std::empty(reqs.front().cmds));

   reqs.front().cmds.pop();
   if (std::empty(reqs.front().cmds)) {
      reqs.pop();
      return true;
   }

   return false;
}

void
forward_transaction(
   resp3::transaction_result& result,
   std::deque<std::pair<command, resp3::type>> const& ids,
   receiver_base& recv)
{
   assert(std::size(ids) == std::size(result));

   for (auto i = 0U; i < std::size(ids); ++i)
      result[i].cmd = ids[i].first;

   recv.on_transaction(result);
}

void
forward(
   response_buffers& buffers,
   command cmd,
   resp3::type type,
   receiver_base& recv)
{
   switch (type) {
      case resp3::type::set:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(buffers.resp_set, smembers);
	    default: {assert(false);}
	 }
	 buffers.resp_set.result->clear();
      } break;
      case resp3::type::map:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(buffers.resp_map, hello);
	    EXPAND_RECEIVER_CASE(buffers.resp_map, hgetall);
	    default: {assert(false);}
	 }
	 buffers.resp_map.result->clear();
      } break;
      case resp3::type::array:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(buffers.resp_array, acl_list);
	    EXPAND_RECEIVER_CASE(buffers.resp_array, acl_users);
	    EXPAND_RECEIVER_CASE(buffers.resp_array, acl_getuser);
	    EXPAND_RECEIVER_CASE(buffers.resp_array, acl_cat);
	    EXPAND_RECEIVER_CASE(buffers.resp_array, acl_log);
	    EXPAND_RECEIVER_CASE(buffers.resp_array, acl_help);
	    EXPAND_RECEIVER_CASE(buffers.resp_array, lrange);
	    EXPAND_RECEIVER_CASE(buffers.resp_array, lpop);
	    EXPAND_RECEIVER_CASE(buffers.resp_array, zrange);
	    EXPAND_RECEIVER_CASE(buffers.resp_array, zrangebyscore);
	    EXPAND_RECEIVER_CASE(buffers.resp_array, hvals);
	    default: {assert(false);}
	 }
	 buffers.resp_array.result->clear();
      } break;
      case resp3::type::simple_string:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(buffers.resp_simple_string, acl_load);
	    EXPAND_RECEIVER_CASE(buffers.resp_simple_string, acl_save);
	    EXPAND_RECEIVER_CASE(buffers.resp_simple_string, acl_setuser);
	    EXPAND_RECEIVER_CASE(buffers.resp_simple_string, acl_log);
	    EXPAND_RECEIVER_CASE(buffers.resp_simple_string, ping);
	    EXPAND_RECEIVER_CASE(buffers.resp_simple_string, quit);
	    EXPAND_RECEIVER_CASE(buffers.resp_simple_string, flushall);
	    EXPAND_RECEIVER_CASE(buffers.resp_simple_string, ltrim);
	    EXPAND_RECEIVER_CASE(buffers.resp_simple_string, set);
	    default: {assert(false);}
	 }
	 buffers.resp_simple_string.result->clear();
      } break;
      case resp3::type::number:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(buffers.resp_number, acl_deluser);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, rpush);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, del);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, llen);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, publish);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, incr);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, append);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, hset);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, hincrby);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, zadd);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, zremrangebyscore);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, expire);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, sadd);
	    EXPAND_RECEIVER_CASE(buffers.resp_number, hdel);
	    default: {assert(false);}
	 }
      } break;
      case resp3::type::doublean:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
      } break;
      case resp3::type::big_number:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
	 buffers.resp_big_number.result->clear();
      } break;
      case resp3::type::boolean:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
	 *buffers.resp_boolean.result = false;
      } break;
      case resp3::type::blob_string:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(buffers.resp_blob_string, acl_genpass);
	    EXPAND_RECEIVER_CASE(buffers.resp_blob_string, acl_whoami);
	    EXPAND_RECEIVER_CASE(buffers.resp_blob_string, lpop);
	    EXPAND_RECEIVER_CASE(buffers.resp_blob_string, get);
	    EXPAND_RECEIVER_CASE(buffers.resp_blob_string, hget);
	    default: {assert(false);}
	 }
	 buffers.resp_blob_string.result->clear();
      } break;
      case resp3::type::verbatim_string:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
	 buffers.resp_verbatim_string.result->clear();
      } break;
      case resp3::type::streamed_string_part:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
	 buffers.resp_streamed_string_part.result->clear();
      } break;
      case resp3::type::simple_error:
      {
	 recv.on_simple_error(cmd, *buffers.resp_simple_error.result);
	 buffers.resp_simple_error.result->clear();
      } break;
      case resp3::type::blob_error:
      {
	 recv.on_blob_error(cmd, *buffers.resp_blob_error.result);
	 buffers.resp_blob_error.result->clear();
      } break;
      case resp3::type::null:
      {
	 recv.on_null(cmd);
      } break;
      case resp3::type::attribute:
      {
	 throw std::runtime_error("Attribute are not supported yet.");
      } break;
      default:
      {
	 assert(false);
      }
   }
}

response_base* select_buffer(response_buffers& buffers, resp3::type type)
{
   switch (type) {
      case resp3::type::set: return &buffers.resp_set;
      case resp3::type::map: return &buffers.resp_map;
      case resp3::type::attribute: return &buffers.resp_attribute;
      case resp3::type::array: return &buffers.resp_array;
      case resp3::type::simple_error: return &buffers.resp_simple_error;
      case resp3::type::simple_string: return &buffers.resp_simple_string;
      case resp3::type::number: return &buffers.resp_number;
      case resp3::type::doublean: return &buffers.resp_double;
      case resp3::type::big_number: return &buffers.resp_big_number;
      case resp3::type::boolean: return &buffers.resp_boolean;
      case resp3::type::blob_error: return &buffers.resp_blob_error;
      case resp3::type::blob_string: return &buffers.resp_blob_string;
      case resp3::type::verbatim_string: return &buffers.resp_verbatim_string;
      case resp3::type::streamed_string_part: return &buffers.resp_streamed_string_part;
      case resp3::type::null: return &buffers.resp_ignore;
      default: {
	 throw std::runtime_error("response_buffers");
	 return nullptr;
      }
   }
}

} // detail
} // aedis
