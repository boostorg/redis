#pragma once

# include <system_error>

/// \file error.hpp

namespace aedis {
namespace resp3 {

/** \brief Error that may occurr when parsing from RESP3.
 *  \ingroup enums
 *
 *  The errors that may occurr when parsing from RESP3.
 */
enum class error
{
   /// Invalid RESP3 type.
   invalid_type = 1,

   /// Can't parse the string as an integer.
   not_a_number,

   /// Received less bytes than expected.
   unexpected_read_size
};

namespace detail {

struct error_category_impl : std::error_category {

   char const* name() const noexcept override
      { return "aedis.resp3"; }

   std::string message(int ev) const override
   {
      switch(static_cast<error>(ev)) {
	 case error::invalid_type: return "Invalid resp3 type.";
	 case error::not_a_number: return "Can't convert string to number.";
	 case error::unexpected_read_size: return "Unexpected read size.";
	 default: assert(false);
      }
   }
};

inline
std::error_category const& category()
{
  static error_category_impl instance;
  return instance;
}

} // detail

/// Converts an error in an std::error_code object.
inline
std::error_code make_error_code(error e)
{
    static detail::error_category_impl const eci{};
    return std::error_code{static_cast<int>(e), detail::category()};
}

inline
std::error_condition make_error_condition(error e)
{
  return std::error_condition(static_cast<int>(e), detail::category());
}

} // resp3
} // aedis

namespace std {

template<>
struct is_error_code_enum<::aedis::resp3::error> : std::true_type {};

} // std
