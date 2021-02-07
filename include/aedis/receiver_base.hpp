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

   // Array
   virtual void on_lrange(Event ev, resp::array_type& v) noexcept { }
   virtual void on_lpop(Event ev, resp::array_type& v) noexcept { }

   // Map
   virtual void on_hello(Event ev, resp::map_type& v) noexcept {}

   // Simple string
   virtual void on_ping(Event ev, resp::simple_string_type& v) noexcept { }
   virtual void on_quit(Event ev, resp::simple_string_type& v) noexcept { }
   virtual void on_flushall(Event ev, resp::simple_string_type& v) noexcept { }
   virtual void on_ltrim(Event ev, resp::simple_string_type& v) noexcept { }
   virtual void on_set(Event ev, resp::simple_string_type& v) noexcept { }

   // Number
   virtual void on_rpush(Event ev, resp::number_type v) noexcept { }
   virtual void on_del(Event ev, resp::number_type v) noexcept { }
   virtual void on_llen(Event ev, resp::number_type v) noexcept { }
   virtual void on_publish(Event ev, resp::number_type v) noexcept { }
   virtual void on_incr(Event ev, resp::number_type v) noexcept { }

   // Blob string
   virtual void on_lpop(Event ev, resp::blob_string_type& v) noexcept { }
   virtual void on_get(Event ev, resp::blob_string_type& v) noexcept { }

   virtual void on_double(command cmd, Event ev, resp::double_type& v) noexcept { }
   virtual void on_big_number(command cmd, Event ev, resp::big_number_type& v) noexcept { }
   virtual void on_boolean(command cmd, Event ev, resp::bool_type& v) noexcept {  }
   virtual void on_verbatim_string(command cmd, Event ev, resp::verbatim_string_type& v) noexcept { }
   virtual void on_streamed_string_part(command cmd, Event ev, resp::streamed_string_part_type& v) noexcept { }
   virtual void on_error(boost::system::error_code ec) { }

   // TODO: Introduce a push type.
   virtual void on_push(Event ev, resp::array_type& v) noexcept { }
   virtual void on_simple_error(command cmd, Event ev, resp::response_simple_error::data_type& v) noexcept { }
   virtual void on_blob_error(command cmd, Event ev, resp::response_blob_error::data_type& v) noexcept { }
   virtual void on_null(command cmd, Event ev) noexcept { }
};

} // aedis
