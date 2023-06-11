/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>

namespace boost::redis {

connection::connection(executor_type ex, asio::ssl::context::method method)
: impl_{ex, method}
{ }

connection::connection(asio::io_context& ioc, asio::ssl::context::method method)
: impl_(ioc.get_executor(), method)
{ }

void
connection::async_run_impl(
   config const& cfg,
   logger l,
   asio::any_completion_handler<void(boost::system::error_code)> token)
{
   impl_.async_run(cfg, l, std::move(token));
}

void connection::cancel(operation op)
{
   impl_.cancel(op);
}

} // namespace boost::redis
