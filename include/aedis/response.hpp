/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/type.hpp>
#include <aedis/command.hpp>
#include <aedis/resp3/adapter_utils.hpp>
#include <aedis/resp3/ignore_adapter.hpp>
#include <aedis/resp3/array_adapter.hpp>
#include <aedis/resp3/flat_map_adapter.hpp>
#include <aedis/resp3/flat_set_adapter.hpp>
#include <aedis/resp3/basic_flat_array_adapter.hpp>
#include <aedis/resp3/number_adapter.hpp>
#include <aedis/resp3/blob_string_adapter.hpp>
#include <aedis/resp3/simple_string_adapter.hpp>
#include <aedis/resp3/blob_error_adapter.hpp>
#include <aedis/resp3/simple_error_adapter.hpp>
#include <aedis/resp3/big_number_adapter.hpp>
#include <aedis/resp3/doublean_adapter.hpp>
#include <aedis/resp3/verbatim_string_adapter.hpp>
#include <aedis/resp3/boolean_adapter.hpp>
#include <aedis/resp3/streamed_string_part_adapter.hpp>

namespace aedis {

class response {
private:
   resp3::array_adapter array_;
   resp3::basic_flat_array_adapter<std::string> flat_array_;
   resp3::basic_flat_array_adapter<std::string> flat_push_;
   resp3::flat_set_adapter flat_set_;
   resp3::flat_map_adapter flat_map_;
   resp3::basic_flat_array_adapter<std::string> flat_attribute_;
   resp3::simple_string_adapter simple_string_;
   resp3::simple_error_adapter simple_error_;
   resp3::number_adapter number_;
   resp3::doublean_adapter doublean_;
   resp3::boolean_adapter boolean_;
   resp3::big_number_adapter big_number_;
   resp3::blob_string_adapter blob_string_;
   resp3::blob_error_adapter blob_error_;
   resp3::verbatim_string_adapter verbatim_string_;
   resp3::streamed_string_part_adapter streamed_string_part_;
   resp3::ignore_adapter ignore_;

public:
   resp3::array array;
   resp3::flat_array flat_array;
   resp3::flat_array flat_push;
   resp3::flat_set flat_set;
   resp3::flat_map flat_map;
   resp3::flat_array flat_attribute;
   resp3::simple_string simple_string;
   resp3::simple_error simple_error;
   resp3::number number;
   resp3::doublean doublean;
   resp3::boolean boolean;
   resp3::big_number big_number;
   resp3::blob_string blob_string;
   resp3::blob_error blob_error;
   resp3::verbatim_string verbatim_string;
   resp3::streamed_string_part streamed_string_part;

   response();
   response_adapter_base* select_adapter(resp3::type type, command cmd);
};

} // aedis
