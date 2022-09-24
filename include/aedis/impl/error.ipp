/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <aedis/error.hpp>

namespace aedis {
namespace detail {

struct error_category_impl : boost::system::error_category {

   virtual ~error_category_impl() = default;

   auto name() const noexcept -> char const* override
   {
      return "aedis";
   }

   auto message(int ev) const -> std::string override
   {
      switch(static_cast<error>(ev)) {
	 case error::resolve_timeout: return "Resolve operation timeout.";
	 case error::connect_timeout: return "Connect operation timeout.";
	 case error::idle_timeout: return "Idle timeout.";
	 case error::exec_timeout: return "Exec timeout.";
	 case error::invalid_data_type: return "Invalid resp3 type.";
	 case error::not_a_number: return "Can't convert string to number.";
	 case error::exceeeds_max_nested_depth: return "Exceeds the maximum number of nested responses.";
	 case error::unexpected_bool_value: return "Unexpected bool value.";
	 case error::empty_field: return "Expected field value is empty.";
	 case error::expects_resp3_simple_type: return "Expects a resp3 simple type.";
	 case error::expects_resp3_aggregate: return "Expects resp3 aggregate.";
	 case error::expects_resp3_map: return "Expects resp3 map.";
	 case error::expects_resp3_set: return "Expects resp3 set.";
	 case error::nested_aggregate_not_supported: return "Nested aggregate not_supported.";
	 case error::resp3_simple_error: return "Got RESP3 simple-error.";
	 case error::resp3_blob_error: return "Got RESP3 blob-error.";
	 case error::incompatible_size: return "Aggregate container has incompatible size.";
	 case error::not_a_double: return "Not a double.";
	 case error::resp3_null: return "Got RESP3 null.";
	 case error::unexpected_server_role: return "Unexpected server role.";
	 case error::ssl_handshake_timeout: return "SSL handshake timeout.";
	 case error::not_connected: return "Not connected.";
	 default: BOOST_ASSERT(false); return "Aedis error.";
      }
   }
};

auto category() -> boost::system::error_category const&
{
  static error_category_impl instance;
  return instance;
}

} // detail

auto make_error_code(error e) -> boost::system::error_code
{
    return boost::system::error_code{static_cast<int>(e), detail::category()};
}

} // aedis
