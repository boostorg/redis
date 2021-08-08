/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <cassert>

#include <aedis/detail/response_buffers.hpp>

namespace aedis { namespace detail {

void response_buffers::forward_transaction(
   std::deque<std::pair<commands, types>> const& ids,
   receiver_base& recv)
{
   assert(std::size(ids) == std::size(tree_.result));

   for (auto i = 0U; i < std::size(ids); ++i)
      tree_.result[i].command = ids[i].first;

   recv.on_transaction(tree_.result);
}

void response_buffers::forward(commands cmd, types t, receiver_base& recv)
{
   switch (t) {
      case types::push:
      {
	 assert(t == types::push);
	 recv.on_push(push_.result);
	 push_.result.clear();
      } break;
      case types::set:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(set_, smembers);
	    default: {assert(false);}
	 }
	 set_.result.clear();
      } break;
      case types::map:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(map_, hello);
	    EXPAND_RECEIVER_CASE(map_, hgetall);
	    default: {assert(false);}
	 }
	 map_.result.clear();
      } break;
      case types::array:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(array_, acl_list);
	    EXPAND_RECEIVER_CASE(array_, acl_users);
	    EXPAND_RECEIVER_CASE(array_, acl_getuser);
	    EXPAND_RECEIVER_CASE(array_, acl_cat);
	    EXPAND_RECEIVER_CASE(array_, acl_log);
	    EXPAND_RECEIVER_CASE(array_, acl_help);
	    EXPAND_RECEIVER_CASE(array_, lrange);
	    EXPAND_RECEIVER_CASE(array_, lpop);
	    EXPAND_RECEIVER_CASE(array_, zrange);
	    EXPAND_RECEIVER_CASE(array_, zrangebyscore);
	    EXPAND_RECEIVER_CASE(array_, hvals);
	    default: {assert(false);}
	 }
	 array_.result.clear();
      } break;
      case types::simple_string:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(simple_string_, acl_load);
	    EXPAND_RECEIVER_CASE(simple_string_, acl_save);
	    EXPAND_RECEIVER_CASE(simple_string_, acl_setuser);
	    EXPAND_RECEIVER_CASE(simple_string_, acl_log);
	    EXPAND_RECEIVER_CASE(simple_string_, ping);
	    EXPAND_RECEIVER_CASE(simple_string_, quit);
	    EXPAND_RECEIVER_CASE(simple_string_, flushall);
	    EXPAND_RECEIVER_CASE(simple_string_, ltrim);
	    EXPAND_RECEIVER_CASE(simple_string_, set);
	    default: {assert(false);}
	 }
	 simple_string_.result.clear();
      } break;
      case types::number:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(number_, acl_deluser);
	    EXPAND_RECEIVER_CASE(number_, rpush);
	    EXPAND_RECEIVER_CASE(number_, del);
	    EXPAND_RECEIVER_CASE(number_, llen);
	    EXPAND_RECEIVER_CASE(number_, publish);
	    EXPAND_RECEIVER_CASE(number_, incr);
	    EXPAND_RECEIVER_CASE(number_, append);
	    EXPAND_RECEIVER_CASE(number_, hset);
	    EXPAND_RECEIVER_CASE(number_, hincrby);
	    EXPAND_RECEIVER_CASE(number_, zadd);
	    EXPAND_RECEIVER_CASE(number_, zremrangebyscore);
	    EXPAND_RECEIVER_CASE(number_, expire);
	    EXPAND_RECEIVER_CASE(number_, sadd);
	    EXPAND_RECEIVER_CASE(number_, hdel);
	    default: {assert(false);}
	 }
      } break;
      case types::double_type:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
      } break;
      case types::big_number:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
	 big_number_.result.clear();
      } break;
      case types::boolean:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
	 bool_.result = false;
      } break;
      case types::blob_string:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(blob_string_, acl_genpass);
	    EXPAND_RECEIVER_CASE(blob_string_, acl_whoami);
	    EXPAND_RECEIVER_CASE(blob_string_, lpop);
	    EXPAND_RECEIVER_CASE(blob_string_, get);
	    EXPAND_RECEIVER_CASE(blob_string_, hget);
	    default: {assert(false);}
	 }
	 blob_string_.result.clear();
      } break;
      case types::verbatim_string:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
	 verbatim_string_.result.clear();
      } break;
      case types::streamed_string_part:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
	 streamed_string_part_.result.clear();
      } break;
      case types::simple_error:
      {
	 recv.on_simple_error(cmd, simple_error_.result);
	 simple_error_.result.clear();
      } break;
      case types::blob_error:
      {
	 recv.on_blob_error(cmd, blob_error_.result);
	 blob_error_.result.clear();
      } break;
      case types::null:
      {
	 recv.on_null(cmd);
      } break;
      case types::attribute:
      {
	 throw std::runtime_error("Attribute are not supported yet.");
      } break;
      default:{}
   }
}

response_base* response_buffers::select(commands cmd, types t)
{
   if (cmd == commands::exec)
     return &tree_;

   switch (t) {
      case types::push: return &push_;
      case types::set: return &set_;
      case types::map: return &map_;
      case types::attribute: return &attribute_;
      case types::array: return &array_;
      case types::simple_error: return &simple_error_;
      case types::simple_string: return &simple_string_;
      case types::number: return &number_;
      case types::double_type: return &double_;
      case types::big_number: return &big_number_;
      case types::boolean: return &bool_;
      case types::blob_error: return &blob_error_;
      case types::blob_string: return &blob_string_;
      case types::verbatim_string: return &verbatim_string_;
      case types::streamed_string_part: return &streamed_string_part_;
      case types::null: return &ignore_;
      default: {
	 throw std::runtime_error("response_buffers");
	 return nullptr;
      }
   }
}

} // detail
} // aedis

