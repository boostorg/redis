/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/response.hpp>
#include <aedis/response_adapter_base.hpp>

namespace aedis { namespace resp3 {

struct simple_error_adapter : public response_adapter_base {
   resp3::simple_error* result = nullptr;

   simple_error_adapter(resp3::simple_error* p) : result(p) {}

   void on_simple_error(std::string_view s) override
      { *result = s; }
};

} // resp3
} // aedis
