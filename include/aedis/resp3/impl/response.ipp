/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/resp3/response.hpp>
#include <aedis/command.hpp>

namespace aedis {
namespace resp3 {

response_adapter_base*
response::select_adapter(
   type type,
   command cmd,
   std::string const&)
{
   data_.resize(0);
   array_adapter_.clear();
   return &array_adapter_;
}

std::ostream& operator<<(std::ostream& os, response const& r)
{
   for (auto const& n : r.data_)
      os << n << "\n";

   return os;
}

} // resp3
} // aedis
