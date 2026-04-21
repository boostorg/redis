#pragma once

#include <boost/redis/config.hpp>
#include <boost/redis/logger.hpp>

#include <boost/system/errc.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/error_condition.hpp>

#include <chrono>
#include <iosfwd>
#include <string>
#include <string_view>
#include <system_error>

// The timeout for tests involving communication to a real server.
// Some tests use a longer timeout by multiplying this value by some
// integral number.
inline constexpr std::chrono::seconds test_timeout{30};

boost::redis::config make_test_config();
std::string get_server_hostname();

// Finds a value in the output of the CLIENT INFO command
// format: key1=value1 key2=value2
std::string_view find_client_info(std::string_view client_info, std::string_view key);

boost::redis::logger make_string_logger(std::string& to);

std::string safe_getenv(const char* name, const char* default_value);

// std::error_condition doesn't implement operator<< and is difficult to use in tests
struct condition_wrapper {
   std::error_condition value;

   friend bool operator==(const condition_wrapper& lhs, const condition_wrapper& rhs) noexcept
   {
      return lhs.value == rhs.value;
   }

   template <class T>
   friend bool operator==(const T& lhs, const condition_wrapper& rhs) noexcept
   {
      return lhs == rhs.value;
   }

   template <class T>
   friend bool operator==(const condition_wrapper& lhs, const T& rhs) noexcept
   {
      return lhs.value == rhs;
   }

   friend std::ostream& operator<<(std::ostream& os, const condition_wrapper& val);
};

// Reduce verbosity in tests
condition_wrapper canceled_condition();
