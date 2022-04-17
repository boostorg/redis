/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/generic/error.hpp>

namespace aedis {
namespace generic {
namespace detail {

struct error_category_impl : boost::system::error_category {

   char const* name() const noexcept override
   {
      return "aedis.generic";
   }

   std::string message(int ev) const override
   {
      switch(static_cast<error>(ev)) {
	 case error::read_timeout: return "Read operation timeout.";
	 case error::write_timeout: return "Write operation timeout.";
	 case error::connect_timeout: return "Connect operation timeout.";
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
    return boost::system::error_code{static_cast<int>(e), detail::category()};
}

boost::system::error_condition make_error_condition(error e)
{
  return boost::system::error_condition(static_cast<int>(e), detail::category());
}

} // generic
} // aedis
