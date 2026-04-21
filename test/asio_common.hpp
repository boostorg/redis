#pragma once

#include <boost/redis/connection.hpp>
#include <boost/redis/detail/reader_fsm.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/operation.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>

#include "common.hpp"

#include <chrono>
#include <memory>
#include <string_view>

#ifdef BOOST_ASIO_HAS_CO_AWAIT
inline auto redir(boost::system::error_code& ec)
{
   return boost::asio::redirect_error(boost::asio::use_awaitable, ec);
}
void run_coroutine_test(
   boost::asio::awaitable<void>,
   std::chrono::steady_clock::duration timeout = test_timeout);
#endif  // BOOST_ASIO_HAS_CO_AWAIT

void run(
   std::shared_ptr<boost::redis::connection> conn,
   boost::redis::config cfg = make_test_config(),
   boost::system::error_code ec = boost::asio::error::operation_aborted,
   boost::redis::operation op = boost::redis::operation::receive);

// Connects to the Redis server at the given port and creates a user
void create_user(std::string_view port, std::string_view username, std::string_view password);
