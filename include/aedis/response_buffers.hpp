/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "type.hpp"
#include "command.hpp"
#include "response_types.hpp"

namespace aedis { namespace resp {

template <class Event>
struct response_id {
   command cmd;
   type t;
   Event event;
};

#define EXPAND_RECEIVER_CASE(var, x) case command::x: recv.on_##x(id.event, var.result); break

class response_buffers {
private:
   // TODO: Use a variant to store all responses.
   response_tree tree_;
   response_array array_;
   response_array push_;
   response_set set_;
   response_map map_;
   response_array attribute_;
   response_simple_string simple_string_;
   response_simple_error simple_error_;
   response_number number_;
   response_double double_;
   response_bool bool_;
   response_big_number big_number_;
   response_blob_string blob_string_;
   response_blob_error blob_error_;
   response_verbatim_string verbatim_string_;
   response_streamed_string_part streamed_string_part_;
   response_ignore ignore_;

public:
   // When the id is from a transaction the type of the message is not
   // specified.
   template <class Event>
   response_base* select(response_id<Event> const& id)
   {
      if (id.cmd == command::exec)
        return &tree_;

      switch (id.t) {
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

   template <
      class Event,
      class Receiver>
   void
   forward_transaction(
      std::queue<response_id<Event>> ids,
      Receiver& recv)
   {
      while (!std::empty(ids)) {
        std::cout << ids.front() << std::endl;
        ids.pop();
      }

      tree_.result.clear();
   }

   template <
      class Event,
      class Receiver>
   void
   forward(
      response_id<Event> const& id,
      Receiver& recv)
   {
      switch (id.t) {
         case type::push:
	 {
	    assert(id.t == type::push);
	    recv.on_push(id.event, push_.result);
	    push_.result.clear();
	 } break;
         case type::set:
	 {
	    //recv.on_set(id.cmd, id.event, set_.result);
	    set_.result.clear();
	 } break;
         case type::map:
	 {
	    switch (id.cmd) {
	       EXPAND_RECEIVER_CASE(map_, hello);
	       EXPAND_RECEIVER_CASE(map_, hgetall);
	       default: {assert(false);}
	    }
	    map_.result.clear();
	 } break;
         case type::array:
	 {
	    switch (id.cmd) {
	       EXPAND_RECEIVER_CASE(array_, lrange);
	       EXPAND_RECEIVER_CASE(array_, lpop);
	       EXPAND_RECEIVER_CASE(array_, zrange);
	       EXPAND_RECEIVER_CASE(array_, zrangebyscore);
	       default: {assert(false);}
	    }
	    array_.result.clear();
	 } break;
         case type::simple_string:
	 {
	    switch (id.cmd) {
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
	    switch (id.cmd) {
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
	       default: {assert(false);}
	    }
	 } break;
         case type::double_type:
	 {
	    switch (id.cmd) {
	       default: {assert(false);}
	    }
	 } break;
         case type::big_number:
	 {
	    switch (id.cmd) {
	       default: {assert(false);}
	    }
	    big_number_.result.clear();
	 } break;
         case type::boolean:
	 {
	    switch (id.cmd) {
	       default: {assert(false);}
	    }
	    bool_.result = false;
	 } break;
         case type::blob_string:
	 {
	    switch (id.cmd) {
	       EXPAND_RECEIVER_CASE(blob_string_, lpop);
	       EXPAND_RECEIVER_CASE(blob_string_, get);
	       EXPAND_RECEIVER_CASE(blob_string_, hget);
	       default: {assert(false);}
	    }
	    blob_string_.result.clear();
	 } break;
	 case type::verbatim_string:
	 {
	    switch (id.cmd) {
	       default: {assert(false);}
	    }
	    verbatim_string_.result.clear();
	 } break;
	 case type::streamed_string_part:
	 {
	    switch (id.cmd) {
	       default: {assert(false);}
	    }
	    streamed_string_part_.result.clear();
	 } break;
         case type::simple_error:
	 {
	    recv.on_simple_error(id.cmd, id.event, simple_error_.result);
	    simple_error_.result.clear();
	 } break;
         case type::blob_error:
	 {
	    recv.on_blob_error(id.cmd, id.event, blob_error_.result);
	    blob_error_.result.clear();
	 } break;
         case type::null:
	 {
	    recv.on_null(id.cmd, id.event);
	 } break;
         case type::attribute:
	 {
	    throw std::runtime_error("Attribute are not supported yet.");
	 } break;
	 default:{}
      }
   }
};

} // resp
} // aedis

template <class Event>
std::ostream&
operator<<(std::ostream& os, aedis::resp::response_id<Event> const& id)
{
   os
      << std::left << std::setw(15) << id.cmd
      << std::left << std::setw(20) << id.t
      << std::left << std::setw(4) << (int)id.event
   ;
   return os;
}
