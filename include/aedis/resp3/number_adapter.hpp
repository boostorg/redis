/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/response.hpp>
#include <aedis/response_adapter_base.hpp>
#include <aedis/resp3/adapter_utils.hpp>

#include <boost/static_string/static_string.hpp>

namespace aedis { namespace resp3 {

struct number_adapter : public response_adapter_base {
   resp3::number* result = nullptr;

   number_adapter(resp3::number* p) : result(p) {}

   void on_number(std::string_view s) override
      { from_string_view(s, *result); }
};

} // resp3
} // aedis
