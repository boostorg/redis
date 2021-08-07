/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <ostream>

namespace aedis { namespace resp {

enum class types
{ array
, push
, set
, map
, attribute
, simple_string
, simple_error
, number
, double_type
, boolean
, big_number
, null
, blob_error
, verbatim_string
, blob_string
, streamed_string_part
, invalid
};

types to_type(char c);
std::ostream& operator<<(std::ostream& os, types t);

} // resp
} // aedis
