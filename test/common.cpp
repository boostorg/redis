#include <boost/redis/config.hpp>

#include <boost/capy/ex/run_async.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/corosio/io_context.hpp>

#include "common.hpp"

#include <chrono>
#include <cstdlib>
#include <string_view>

using namespace std::chrono_literals;

static std::string safe_getenv(const char* name, const char* default_value)
{
   // MSVC doesn't like getenv
#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
   const char* res = std::getenv(name);
#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
   return res ? res : default_value;
}

std::string get_server_hostname() { return safe_getenv("BOOST_REDIS_TEST_SERVER", "localhost"); }

boost::redis::config make_test_config()
{
   boost::redis::config cfg;
   cfg.addr.host = get_server_hostname();
   cfg.reconnect_wait_interval = 50ms;  // make tests involving reconnection faster
   return cfg;
}

// Finds a value in the output of the CLIENT INFO command
// format: key1=value1 key2=value2
std::string_view find_client_info(std::string_view client_info, std::string_view key)
{
   std::string prefix{key};
   prefix += '=';

   auto const pos = client_info.find(prefix);
   if (pos == std::string_view::npos)
      return {};
   auto const pos_begin = pos + prefix.size();
   auto const pos_end = client_info.find(' ', pos_begin);
   return client_info.substr(pos_begin, pos_end - pos_begin);
}

boost::redis::logger make_string_logger(std::string& to)
{
   return {
      boost::redis::logger::level::info,
      [&to](boost::redis::logger::level, std::string_view msg) {
         to += msg;
         to += '\n';
      }};
}

void run_coroutine_test(boost::capy::task<void> test)
{
   // TODO: test timeout
   boost::corosio::io_context ctx;
   boost::capy::run_async(ctx.get_executor())(std::move(test));
   ctx.run();
}
