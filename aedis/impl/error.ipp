/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <aedis/error.hpp>

namespace aedis {
namespace detail {

struct error_category_impl : boost::system::error_category {

   char const* name() const noexcept override
   {
      return "aedis";
   }

   std::string message(int ev) const override
   {
      switch(static_cast<error>(ev)) {
	 case error::resolve_timeout: return "Resolve operation timeout.";
	 case error::connect_timeout: return "Connect operation timeout.";
	 case error::read_timeout: return "Read operation timeout.";
	 case error::write_timeout: return "Write operation timeout.";
	 case error::idle_timeout: return "Idle timeout.";
	 case error::invalid_data_type: return "Invalid resp3 type.";
	 case error::not_a_number: return "Can't convert string to number.";
	 case error::unexpected_read_size: return "Unexpected read size.";
	 case error::exceeeds_max_nested_depth: return "Exceeds the maximum number of nested responses.";
	 case error::unexpected_bool_value: return "Unexpected bool value.";
	 case error::empty_field: return "Expected field value is empty.";
	 case error::expects_simple_type: return "Expects a simple RESP3 type.";
	 case error::expects_aggregate_type: return "Expects aggregate type.";
	 case error::expects_map_type: return "Expects map type.";
	 case error::expects_set_type: return "Expects set type.";
	 case error::nested_aggregate_unsupported: return "Nested aggregate unsupported.";
	 case error::simple_error: return "Got RESP3 simple-error.";
	 case error::blob_error: return "Got RESP3 blob-error.";
	 case error::incompatible_size: return "Aggregate container has incompatible size.";
	 case error::not_a_double: return "Not a double.";
	 case error::null: return "Got RESP3 null.";
	 default:
            BOOST_ASSERT(false);
            return "Aedis error.";
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

} // aedis
