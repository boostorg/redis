/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/detail/adapter_utils.hpp>
#include <aedis/resp3/detail/ignore_adapter.hpp>
#include <aedis/resp3/detail/array_adapter.hpp>
#include <aedis/resp3/detail/basic_flat_array_adapter.hpp>
#include <aedis/resp3/detail/number_adapter.hpp>
#include <aedis/resp3/detail/boolean_adapter.hpp>

namespace aedis { namespace resp3 {

class response {
private:
   array_type array_;
   detail::array_adapter array_adapter_{&array_};

   flat_array_type flat_array_;
   detail::basic_flat_array_adapter<std::string> flat_array_adapter_{&flat_array_};

   flat_array_type flat_attribute_;
   detail::basic_flat_array_adapter<std::string> flat_attribute_adapter_{&flat_attribute_};

   boolean_type boolean_;
   detail::boolean_adapter boolean_adapter_{&boolean_};

   number_type number_;
   detail::number_adapter number_adapter_{&number_};

   detail::ignore_adapter ignore_adapter_;

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

   auto const& flat_array() const noexcept {return flat_array_;}
   auto& flat_array() noexcept {return flat_array_;}

   auto const& number() const noexcept {return number_;}
   auto& number() noexcept {return number_;}

   auto const& boolean() const noexcept {return boolean_;}
   auto& boolean() noexcept {return boolean_;}
};

} // resp3
} // aedis
