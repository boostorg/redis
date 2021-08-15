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

#define EXPAND_RECEIVER_CASE(var, x) case command::x: recv.on_##x(var.result); break

struct response_buffers {
   // Consider a variant to store all responses.
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
};

response_base* select_buffer(response_buffers& buffers, resp3::type t);

void forward_transaction(
   resp3::transaction_result& result,
   std::deque<std::pair<command, resp3::type>> const& ids,
   receiver_base& recv);

void forward(
   response_buffers& buffers,
   command cmd,
   resp3::type type,
   receiver_base& recv);

} // detail
} // aedis
