/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/response.hpp>
#include <aedis/command.hpp>

namespace aedis {

response::response()
: array_{&array}
, flat_array_{&flat_array}
, flat_push_{&flat_push}
, flat_set_{&flat_set}
, flat_map_{&flat_map}
, flat_attribute_{&flat_attribute}
, simple_string_{&simple_string}
, simple_error_{&simple_error}
, number_{&number}
, doublean_{&doublean}
, boolean_{&boolean}
, big_number_{&big_number}
, blob_string_{&blob_string}
, blob_error_{&blob_error}
, verbatim_string_{&verbatim_string}
, streamed_string_part_{&streamed_string_part}
{ }

response_adapter_base* response::select_adapter(resp3::type type, command cmd)
{
   if (type == resp3::type::flat_push)
     return &flat_push_;

   if (cmd == command::exec)
     return &array_;

   switch (type) {
      case resp3::type::flat_set: return &flat_set_;
      case resp3::type::flat_map: return &flat_map_;
      case resp3::type::flat_attribute: return &flat_attribute_;
      case resp3::type::flat_array: return &flat_array_;
      case resp3::type::simple_error: return &simple_error_;
      case resp3::type::simple_string: return &simple_string_;
      case resp3::type::number: return &number_;
      case resp3::type::doublean: return &doublean_;
      case resp3::type::big_number: return &big_number_;
      case resp3::type::boolean: return &boolean_;
      case resp3::type::blob_error: return &blob_error_;
      case resp3::type::blob_string: return &blob_string_;
      case resp3::type::verbatim_string: return &verbatim_string_;
      case resp3::type::streamed_string_part: return &streamed_string_part_;
      case resp3::type::null: return &ignore_;
      default: {
	 assert(false);
	 return nullptr;
      }
   }
}
} // aedis
