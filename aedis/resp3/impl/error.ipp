/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/resp3/error.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

struct error_category_impl : boost::system::error_category {

   char const* name() const noexcept override
   {
      static char name[] = "aedis.resp3";
      return name;
   }

   std::string message(int ev) const override
   {
      switch(static_cast<error>(ev)) {
	 case error::invalid_type: return "Invalid resp3 type.";
	 case error::not_a_number: return "Can't convert string to number.";
	 case error::unexpected_read_size: return "Unexpected read size.";
	 case error::exceeeds_max_nested_depth: return "Exceeds the maximum number of nested responses.";
	 case error::unexpected_bool_value: return "Unexpected bool value.";
	 case error::empty_field: return "Expected field value is empty.";
	 default: assert(false);
      }
   }
};

boost::system::error_category const& category()
{
  static error_category_impl instance;
  return instance;
}

} // detail

boost::system::error_code make_error_code(error e)
{
    static detail::error_category_impl const eci{};
    return boost::system::error_code{static_cast<int>(e), detail::category()};
}

boost::system::error_condition make_error_condition(error e)
{
  return boost::system::error_condition(static_cast<int>(e), detail::category());
}

} // resp3
} // aedis
