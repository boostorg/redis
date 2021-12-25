#pragma once

# include <system_error>

// Errors that may occurr while parsing resp3 messages.

namespace aedis {
namespace resp3 {

/// Error from RESP3 parser.
enum class error
{
   /// Invalid RESP3 type.
   invalid_type = 1,

   /// Can't parse the string in an integer.
   not_an_int,
};

namespace detail {

struct error_category_impl : std::error_category {

   char const* name() const noexcept override
      { return "aedis"; }

   std::string message(int ev) const override
   {
      switch(static_cast<error>(ev)) {
	 case error::invalid_type: return "Invalid resp3 type";
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

