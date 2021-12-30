#pragma once

# include <system_error>


/// \file adapter_error.hpp

namespace aedis {
namespace resp3 {

/** \brief Error that may occurr while processing a response.
 *
 *  The errors that may occurr while processing a response.
 */
enum class adapter_error
{
   /// Expects a simple RESP3 type but e.g. got an aggregate.
   expects_simple_type = 1,

   /// Unexpect depth in the response tree.
   nested_unsupported,

   /// RESP3 simple error.
   simple_error,

   /// RESP3 blob_error.
   blob_error
};

namespace detail {

struct adapter_error_category_impl : std::error_category {

   /// \todo Fix string lifetime.
   char const* name() const noexcept override
      { return "aedis.response_adapter"; }

   std::string message(int ev) const override
   {
      switch(static_cast<adapter_error>(ev)) {
	 case adapter_error::expects_simple_type: return "Expects a simple RESP3 type";
	 case adapter_error::nested_unsupported: return "Nested response elements are unsupported.";
	 case adapter_error::simple_error: return "Got RESP3 simple-error type.";
	 case adapter_error::blob_error: return "Got RESP3 blob-error type.";
	 default: assert(false);
      }
   }
};

inline
std::error_category const& adapter_category()
{
  static adapter_error_category_impl instance;
  return instance;
}

} // detail

inline
std::error_code make_error_code(adapter_error e)
{
    static detail::adapter_error_category_impl const eci{};
    return std::error_code{static_cast<int>(e), detail::adapter_category()};
}

inline
std::error_condition make_error_condition(adapter_error e)
{
  return std::error_condition(static_cast<int>(e), detail::adapter_category());
}

} // resp3
} // aedis

namespace std {

template<>
struct is_error_code_enum<::aedis::resp3::adapter_error> : std::true_type {};

} // std
