/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
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
	 case error::resolve_timeout: return "Resolve operation timeout.";
	 case error::connect_timeout: return "Connect operation timeout.";
	 case error::read_timeout: return "Read operation timeout.";
	 case error::write_timeout: return "Write operation timeout.";
	 case error::idle_timeout: return "Idle timeout.";
	 case error::write_stop_requested: return "Write stop requested.";
	 default: BOOST_ASSERT(false);
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

} // generic
} // aedis
