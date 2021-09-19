/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/type.hpp>
#include <aedis/response_adapter_base.hpp>
#include <aedis/resp3/adapter_utils.hpp>

namespace aedis { namespace resp3 {

struct big_number_adapter : public response_adapter_base {
   big_number* result = nullptr;

   big_number_adapter(big_number* p) : result(p) {}

   void on_big_number(std::string_view s) override
      { from_string_view(s, *result); }
};

} // resp3
} // aedis
