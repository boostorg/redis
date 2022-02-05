#pragma once

# include <system_error>

namespace aedis {
namespace resp3 {
namespace adapter {

/** \brief Errors that may occurr when reading a response.
 *  \ingroup any
 */
enum class error
{
   /// Expects a simple RESP3 type but got an aggregate.
   expects_simple_type = 1,

   /// Nested response not supported.
   nested_unsupported,

   /// Got RESP3 simple error.
   simple_error,

   /// Got RESP3 blob_error.
   blob_error,

   /// The tuple used as response has incompatible size.
   incompatible_tuple_size,

   /// Got RESP3 null type.
   null
};

namespace detail {

struct error_category_impl : std::error_category {

   /// \todo Fix string lifetime.
   char const* name() const noexcept override
      { return "aedis.response_adapter"; }

   // TODO: Move to .ipp
   std::string message(int ev) const override
   {
      switch(static_cast<error>(ev)) {
	 case error::expects_simple_type: return "Expects a simple RESP3 type";
	 case error::nested_unsupported: return "Nested responses unsupported.";
	 case error::simple_error: return "Got RESP3 simple-error.";
	 case error::blob_error: return "Got RESP3 blob-error.";
	 case error::incompatible_tuple_size: return "The tuple used as response has incompatible size.";
	 case error::null: return "Got RESP3 null.";
	 default: assert(false);
      }
   }
};

inline
std::error_category const& adapter_category()
{
  static error_category_impl instance;
  return instance;
}

} // detail

inline
std::error_code make_error_code(error e)
{
    static detail::error_category_impl const eci{};
    return std::error_code{static_cast<int>(e), detail::adapter_category()};
}

inline
std::error_condition make_error_condition(error e)
{
  return std::error_condition(static_cast<int>(e), detail::adapter_category());
}

} // adapter
} // resp3
} // aedis

namespace std {

template<>
struct is_error_code_enum<::aedis::resp3::adapter::error> : std::true_type {};

} // std
