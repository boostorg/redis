/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <iostream>

#include "type.hpp"
#include "utils.hpp"
#include "response.hpp"
#include "request.hpp"

namespace aedis { namespace resp {

class receiver_print {
private:
   response_buffers& buffer_;

public:
   receiver_print(response_buffers& buffer)
   : buffer_{buffer}
   {}

   // The ids in the queue parameter have an unspecified message type.
   template <class Event>
   void receive_transaction(std::queue<response_id<Event>> ids)
   {
      while (!std::empty(ids)) {
        std::cout << ids.front() << std::endl;
        ids.pop();
      }
   }

   template <class Event>
   void receive(response_id<Event> const& id)
   {
      buffer_.tree().clear();

      std::cout << id;
      switch (id.t) {
         case type::push:
	    buffer_.push().clear();
	    break;
         case type::set:
	    buffer_.set().clear();
	    break;
         case type::map:
	    buffer_.map().clear();
	    break;
         case type::attribute:
	    buffer_.attribute().clear();
	    break;
         case type::array:
	    buffer_.array().clear();
	    break;
         case type::simple_error:
	    buffer_.simple_error().clear();
	    break;
         case type::simple_string:
	    buffer_.simple_string().clear();
	    break;
         case type::number:
	    break;
         case type::double_type:
	    break;
         case type::big_number:
	    buffer_.big_number().clear();
	    break;
         case type::boolean:
	    break;
         case type::blob_error:
	    buffer_.blob_error().clear();
	    break;
         case type::blob_string:
	    buffer_.blob_string().clear();
	    break;
	 case type::verbatim_string:
	    buffer_.verbatim_string().clear();
	    break;
	 case type::streamed_string_part:
	    buffer_.streamed_string_part().clear();
	    break;
	 default:{}
      }
      std::cout << std::endl;
   }
};

}
}
