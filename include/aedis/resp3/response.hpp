/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/node.hpp>
#include <aedis/resp3/detail/array_adapter.hpp>

namespace aedis { namespace resp3 {

class response {
private:
   response_impl array_;
   detail::array_adapter array_adapter_{&array_};

public:
   virtual ~response() = default;

   virtual
   response_adapter_base*
   select_adapter(
      resp3::type type,
      command cmd = command::unknown,
      std::string const& key = "");

   auto const& array() const noexcept {return array_;}
   auto& array() noexcept {return array_;}
};

} // resp3
} // aedis
