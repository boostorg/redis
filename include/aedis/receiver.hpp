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

template <class Event>
class receiver_base {
public:
   using event_type = Event;

   virtual void on_lrange(Event ev, resp::response_array::data_type& v) noexcept { }
   virtual void on_hello(Event ev, resp::response_array::data_type& v) noexcept {}
   virtual void on_ping(Event ev, resp::response_simple_string::data_type& v) noexcept { }
   virtual void on_rpush(Event ev, resp::response_number::data_type& v) noexcept { }

   virtual void on_push(command cmd, Event ev, resp::response_array::data_type& v) noexcept { }
   virtual void on_set(command cmd, Event ev, resp::response_array::data_type& v) noexcept { }
   virtual void on_attribute(command cmd, Event ev, resp::response_array::data_type& v) noexcept { }
   virtual void on_simple_error(command cmd, Event ev, resp::response_simple_error::data_type& v) noexcept { }
   virtual void on_double(command cmd, Event ev, resp::response_double::data_type& v) noexcept { }
   virtual void on_big_number(command cmd, Event ev, resp::response_big_number::data_type& v) noexcept { }
   virtual void on_boolean(command cmd, Event ev, resp::response_bool::data_type& v) noexcept {  }
   virtual void on_blob_string(command cmd, Event ev, resp::response_blob_string::data_type& v) noexcept { }
   virtual void on_blob_error(command cmd, Event ev, resp::response_blob_error::data_type& v) noexcept { }
   virtual void on_verbatim_string(command cmd, Event ev, resp::response_verbatim_string::data_type& v) noexcept { }
   virtual void on_streamed_string_part(command cmd, Event ev, resp::response_streamed_string_part::data_type& v) noexcept { }
   virtual void on_error(boost::system::error_code ec) { }
};

} // aedis
