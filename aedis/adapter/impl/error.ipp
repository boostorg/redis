/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <system_error>
#include <boost/assert.hpp>

namespace aedis {
namespace adapter {
namespace detail {

struct error_category_impl : boost::system::error_category {

   char const* name() const noexcept override
   {
      return "aedis.adapter";
   }

   std::string message(int ev) const override
   {
      switch(static_cast<error>(ev)) {
	 case error::expects_simple_type: return "Expects a simple RESP3 type";
	 case error::expects_aggregate: return "Expects aggregate type";
	 case error::expects_map_like_aggregate: return "Expects map aggregate";
	 case error::expects_set_aggregate: return "Expects set aggregate";
	 case error::nested_aggregate_unsupported: return "Nested aggregate unsupported.";
	 case error::simple_error: return "Got RESP3 simple-error.";
	 case error::blob_error: return "Got RESP3 blob-error.";
	 case error::incompatible_size: return "Aggregate container has incompatible size.";
	 case error::not_a_double: return "Not a double.";
	 case error::null: return "Got RESP3 null.";
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

boost::system::error_condition make_error_condition(error e)
{
  return boost::system::error_condition(static_cast<int>(e), detail::category());
}

} // adapter
} // aedis
