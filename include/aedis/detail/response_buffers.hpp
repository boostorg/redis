/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/type.hpp>
#include <aedis/receiver_base.hpp>
#include <aedis/command.hpp>

#include "responses.hpp"

namespace aedis { namespace detail {

struct buffers {
   resp3::transaction_result tree;
   resp3::array array;
   resp3::array push;
   resp3::set set;
   resp3::map map;
   resp3::array attribute;
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
};

struct response_buffers {
   response_tree resp_tree;
   response_array resp_array;
   response_array resp_push;
   response_set resp_set;
   response_map resp_map;
   response_array resp_attribute;
   response_simple_string resp_simple_string;
   response_simple_error resp_simple_error;
   response_number resp_number;
   response_double resp_double;
   response_bool resp_boolean;
   response_big_number resp_big_number;
   response_blob_string resp_blob_string;
   response_blob_error resp_blob_error;
   response_verbatim_string resp_verbatim_string;
   response_streamed_string_part resp_streamed_string_part;
   response_ignore resp_ignore;

   response_buffers(buffers& buf)
   : resp_tree{&buf.tree}
   , resp_array{&buf.array}
   , resp_push{&buf.push}
   , resp_set{&buf.set}
   , resp_map{&buf.map}
   , resp_attribute{&buf.attribute}
   , resp_simple_string{&buf.simple_string}
   , resp_simple_error{&buf.simple_error}
   , resp_number{&buf.number}
   , resp_double{&buf.doublean}
   , resp_boolean{&buf.boolean}
   , resp_big_number{&buf.big_number}
   , resp_blob_string{&buf.blob_string}
   , resp_blob_error{&buf.blob_error}
   , resp_verbatim_string{&buf.verbatim_string}
   , resp_streamed_string_part{&buf.streamed_string_part}
   { }
};

} // detail
} // aedis
