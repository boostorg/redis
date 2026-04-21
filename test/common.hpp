#pragma once

#include <boost/redis/config.hpp>
#include <boost/redis/logger.hpp>

#include <boost/system/error_code.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <string_view>

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
