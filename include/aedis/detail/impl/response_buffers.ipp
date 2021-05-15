/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <cassert>

#include <aedis/detail/response_buffers.hpp>

namespace aedis { namespace resp {

void response_buffers::forward_transaction(
   std::queue<std::pair<command, type>> ids,
   receiver_base& recv)
{
   while (!std::empty(ids)) {
     //std::cout << ids.front() << std::endl;
     ids.pop();
   }

   tree_.result.clear();
}

void response_buffers::forward(command cmd, type t, receiver_base& recv)
{
   switch (t) {
      case type::push:
      {
	 assert(t == type::push);
	 recv.on_push(push_.result);
	 push_.result.clear();
      } break;
      case type::set:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(set_, smembers);
	    default: {assert(false);}
	 }
	 set_.result.clear();
      } break;
      case type::map:
      {
	 switch (cmd) {
	    EXPAND_RECEIVER_CASE(map_, hello);
	    EXPAND_RECEIVER_CASE(map_, hgetall);
	    default: {assert(false);}
	 }
	 map_.result.clear();
      } break;
      case type::array:
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
      case type::simple_string:
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
      case type::number:
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
	    default: {assert(false);}
	 }
      } break;
      case type::double_type:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
      } break;
      case type::big_number:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
	 big_number_.result.clear();
      } break;
      case type::boolean:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
	 bool_.result = false;
      } break;
      case type::blob_string:
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
      case type::verbatim_string:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
	 verbatim_string_.result.clear();
      } break;
      case type::streamed_string_part:
      {
	 switch (cmd) {
	    default: {assert(false);}
	 }
	 streamed_string_part_.result.clear();
      } break;
      case type::simple_error:
      {
	 recv.on_simple_error(cmd, simple_error_.result);
	 simple_error_.result.clear();
      } break;
      case type::blob_error:
      {
	 recv.on_blob_error(cmd, blob_error_.result);
	 blob_error_.result.clear();
      } break;
      case type::null:
      {
	 recv.on_null(cmd);
      } break;
      case type::attribute:
      {
	 throw std::runtime_error("Attribute are not supported yet.");
      } break;
      default:{}
   }
}

response_base* response_buffers::select(command cmd, type t)
{
   if (cmd == command::exec)
     return &tree_;

   switch (t) {
      case type::push: return &push_;
      case type::set: return &set_;
      case type::map: return &map_;
      case type::attribute: return &attribute_;
      case type::array: return &array_;
      case type::simple_error: return &simple_error_;
      case type::simple_string: return &simple_string_;
      case type::number: return &number_;
      case type::double_type: return &double_;
      case type::big_number: return &big_number_;
      case type::boolean: return &bool_;
      case type::blob_error: return &blob_error_;
      case type::blob_string: return &blob_string_;
      case type::verbatim_string: return &verbatim_string_;
      case type::streamed_string_part: return &streamed_string_part_;
      case type::null: return &ignore_;
      default: {
	 throw std::runtime_error("response_buffers");
	 return nullptr;
      }
   }
}

} // resp
} // aedis

