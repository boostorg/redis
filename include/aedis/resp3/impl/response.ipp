/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/resp3/response.hpp>
#include <aedis/command.hpp>

namespace aedis { namespace resp3 {

response_adapter_base*
response::select_adapter(type type, command cmd, std::string const&)
{
   if (type == type::flat_push)
     return &flat_push_adapter_;

   if (cmd == command::exec) {
      array_.resize(0);
      array_adapter_.clear();
      return &array_adapter_;
   }

   switch (type) {
      case type::set:
      case type::map:
      case type::simple_string:
      case type::blob_string:
      {
	 array_.resize(0);
	 array_adapter_.clear();
	 return &array_adapter_;
      } 
      case type::flat_attribute: return &flat_attribute_adapter_;
      case type::flat_array: return &flat_array_adapter_;
      case type::simple_error: return &simple_error_adapter_;
      case type::number: return &number_adapter_;
      case type::doublean: return &doublean_adapter_;
      case type::big_number: return &big_number_adapter_;
      case type::boolean: return &boolean_adapter_;
      case type::blob_error: return &blob_error_adapter_;
      case type::verbatim_string: return &verbatim_string_adapter_;
      case type::streamed_string_part: return &streamed_string_part_adapter_;
      case type::null: return &ignore_adapter_;
      default: {
	 assert(false);
	 return nullptr;
      }
   }
}

} // resp3
} // aedis
