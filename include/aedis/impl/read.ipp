/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/read.hpp>

namespace aedis {

response_adapter_base* select_buffer(detail::response_adapters& adapters, resp3::type type, command cmd)
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

} // aedis
