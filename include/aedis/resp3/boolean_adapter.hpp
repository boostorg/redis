/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/type.hpp>
#include <aedis/response_adapter_base.hpp>

namespace aedis { namespace resp3 {

struct boolean_adapter : public response_adapter_base {
   boolean* result = nullptr;

   boolean_adapter(boolean* p) : result(p) {}

   void on_bool(std::string_view s) override
   {
      assert(std::ssize(s) == 1);
      *result = s[0] == 't';
   }
};

} // resp3
} // aedis
