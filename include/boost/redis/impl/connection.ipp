/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>

#include <cstddef>
#include <string_view>
#include <utility>

namespace boost::redis {

static void log_to_stderr(std::string_view msg, const char* prefix = "(Boost.Redis) ")
{
   // If the message is empty, data() might return a null pointer
   const char* msg_ptr = msg.empty() ? "" : msg.data();

   // Precision should be an int when passed to fprintf. Technically,
   // message could be larger than INT_MAX. Impose a sane limit on message sizes
   // to prevent memory problems
   int precision = (std::min)(msg.size(), static_cast<std::size_t>(0xffffu));

   // Log the message. None of our messages should contain NULL bytes, so this should be OK.
   // We choose fprintf over std::clog because it's safe in multi-threaded environments.
   std::fprintf(stderr, "%s%.*s\n", prefix, precision, msg_ptr);
}

// TODO: test edge cases here
logger detail::make_stderr_logger(logger::level lvl, std::string prefix)
{
   return logger(lvl, [prefix = std::move(prefix)](logger::level, std::string_view msg) {
      log_to_stderr(msg, prefix.c_str());
   });
}

logger detail::default_logger()
{
   return logger(logger::level::info, [](logger::level, std::string_view msg) {
      log_to_stderr(msg);
   });
}

connection::connection(executor_type ex, asio::ssl::context ctx, logger lgr)
: impl_{std::move(ex), std::move(ctx), std::move(lgr)}
{ }

void connection::async_run_impl(
   config const& cfg,
   logger&& l,
   asio::any_completion_handler<void(boost::system::error_code)> token)
{
   impl_.async_run(cfg, std::move(l), std::move(token));
}

void connection::async_run_impl(
   config const& cfg,
   asio::any_completion_handler<void(boost::system::error_code)> token)
{
   impl_.async_run(cfg, std::move(token));
}

void connection::async_exec_impl(
   request const& req,
   any_adapter&& adapter,
   asio::any_completion_handler<void(boost::system::error_code, std::size_t)> token)
{
   impl_.async_exec(req, std::move(adapter), std::move(token));
}

void connection::cancel(operation op) { impl_.cancel(op); }

}  // namespace boost::redis
