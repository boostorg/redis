/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <system_error>

namespace aedis {
namespace adapter {
namespace detail {

struct error_category_impl : boost::system::error_category {

   char const* name() const noexcept override
   {
      static char name[] = "aedis.adapter";
      return name;
   }

   std::string message(int ev) const override
   {
      switch(static_cast<error>(ev)) {
	 case error::expects_simple_type: return "Expects a simple RESP3 type";
	 case error::expects_aggregate: return "Expects aggregate type";
	 case error::nested_unsupported: return "Nested responses unsupported.";
	 case error::simple_error: return "Got RESP3 simple-error.";
	 case error::blob_error: return "Got RESP3 blob-error.";
	 case error::incompatible_tuple_size: return "The tuple used as response has incompatible size.";
	 case error::null: return "Got RESP3 null.";
	 default: assert(false);
      }
   }
};

boost::system::error_category const& adapter_category()
{
  static error_category_impl instance;
  return instance;
}

} // detail

boost::system::error_code make_error_code(error e)
{
    static detail::error_category_impl const eci{};
    return boost::system::error_code{static_cast<int>(e), detail::adapter_category()};
}

boost::system::error_condition make_error_condition(error e)
{
  return boost::system::error_condition(static_cast<int>(e), detail::adapter_category());
}

} // adapter
} // aedis
