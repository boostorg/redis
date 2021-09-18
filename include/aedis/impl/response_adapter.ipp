/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/response_adapter.hpp>

namespace aedis {

response_adapter_base*
select_adapter(response_adapter& adapters, resp3::type type, command cmd)
{
   if (type == resp3::type::flat_push)
     return &adapters.flat_push;

   if (cmd == command::exec)
     return &adapters.array;

   switch (type) {
      case resp3::type::flat_set: return &adapters.flat_set;
      case resp3::type::flat_map: return &adapters.flat_map;
      case resp3::type::flat_attribute: return &adapters.flat_attribute;
      case resp3::type::flat_array: return &adapters.flat_array;
      case resp3::type::simple_error: return &adapters.simple_error;
      case resp3::type::simple_string: return &adapters.simple_string;
      case resp3::type::number: return &adapters.number;
      case resp3::type::doublean: return &adapters.doublean;
      case resp3::type::big_number: return &adapters.big_number;
      case resp3::type::boolean: return &adapters.boolean;
      case resp3::type::blob_error: return &adapters.blob_error;
      case resp3::type::blob_string: return &adapters.blob_string;
      case resp3::type::verbatim_string: return &adapters.verbatim_string;
      case resp3::type::streamed_string_part: return &adapters.streamed_string_part;
      case resp3::type::null: return &adapters.resp_ignore;
      default: {
	 throw std::runtime_error("response_buffers");
	 return nullptr;
      }
   }
}

} // aedis
