/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>

#include <cstddef>
#include <utility>

namespace boost::redis {

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
