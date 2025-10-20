/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/impl/log_to_file.hpp>

#include <cstddef>
#include <cstdio>
#include <string_view>
#include <utility>

namespace boost::redis {

logger detail::make_stderr_logger(logger::level lvl, std::string prefix)
{
   return logger(lvl, [prefix = std::move(prefix)](logger::level, std::string_view msg) {
      log_to_file(stderr, msg, prefix.c_str());
   });
}

system::error_code detail::translate_parallel_group_errors(
   std::array<std::size_t, 3u> order,
   system::error_code setup_ec,
   system::error_code reader_ec,
   system::error_code writer_ec)
{
   // The setup request is special: it might complete successfully,
   // without causing the other tasks to exit.
   // The other tasks will always complete with an error.

   // If the setup task errored and was the first to exit, use its code
   if (order[0] == 0u && setup_ec) {
      return setup_ec;
   }

   // Use the code corresponding to the task that finished first,
   // excluding the setup task
   std::size_t task_number = order[0] == 0u ? order[1] : order[0];
   switch (task_number) {
      case 1u: return reader_ec;
      case 2u: return writer_ec;
      default: BOOST_ASSERT(false); return system::error_code();
   }
}

connection::connection(executor_type ex, asio::ssl::context ctx, logger lgr)
: impl_{std::move(ex), std::move(ctx), std::move(lgr)}
{ }

void connection::async_run_impl(
   config const& cfg,
   logger&& l,
   asio::any_completion_handler<void(boost::system::error_code)> token)
{
   // Avoid calling the basic_connection::async_run overload taking a logger
   // because it generates deprecated messages when building this file
   impl_.set_stderr_logger(l.lvl, cfg);
   impl_.async_run(cfg, std::move(token));
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
