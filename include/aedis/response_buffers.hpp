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

class response_buffers {
private:
   // TODO: Use a variant to store all responses.
   response_tree tree_;
   response_array array_;
   response_array push_;
   response_array set_;
   response_array map_;
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
   response_base* get(response_id<Event> const& id)
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
	    assert(id.t == type::invalid);
	    recv.on_push(id.event, push_.result);
	    push_.result.clear();
	 } break;
         case type::set:
	 {
	    recv.on_set(id.cmd, id.event, set_.result);
	    set_.result.clear();
	 } break;
         case type::map:
	 {
	    switch (id.cmd) {
	       case command::hello: recv.on_hello(id.event, map_.result); break;
	       default: {assert(false);}
	    }
	    map_.result.clear();
	 } break;
         case type::array:
	 {
	    switch (id.cmd) {
	       case command::lrange: recv.on_lrange(id.event, array_.result); break;
	       default: {assert(false);}
	    }
	    array_.result.clear();
	 } break;
         case type::simple_string:
	 {
	    switch (id.cmd) {
	       case command::ping: recv.on_ping(id.event, simple_string_.result); break;
	       case command::quit: recv.on_quit(id.event, simple_string_.result); break;
	       default: {assert(false);}
	    }
	    simple_string_.result.clear();
	 } break;
         case type::number:
	 {
	    switch (id.cmd) {
	       case command::rpush: recv.on_rpush(id.event, number_.result); break;
	       default: {assert(false);}
	    }
	 } break;
         case type::double_type:
	 {
	    recv.on_double(id.cmd, id.event, double_.result);
	 } break;
         case type::big_number:
	 {
	    recv.on_big_number(id.cmd, id.event, big_number_.result);
	    big_number_.result.clear();
	 } break;
         case type::boolean:
	 {
	    recv.on_boolean(id.cmd, id.event, bool_.result);
	    bool_.result = false;
	 } break;
         case type::blob_string:
	 {
	    recv.on_blob_string(id.cmd, id.event, blob_string_.result);
	    blob_string_.result.clear();
	 } break;
	 case type::verbatim_string:
	 {
	    recv.on_verbatim_string(id.cmd, id.event, verbatim_string_.result);
	    verbatim_string_.result.clear();
	 } break;
	 case type::streamed_string_part:
	 {
	    recv.on_streamed_string_part(id.cmd, id.event, streamed_string_part_.result);
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
