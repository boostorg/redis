#pragma once

#include <boost/system/error_code.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/operation.hpp>
#include <memory>

namespace net = boost::asio;

#ifdef BOOST_ASIO_HAS_CO_AWAIT

inline
auto redir(boost::system::error_code& ec)
   { return net::redirect_error(boost::asio::use_awaitable, ec); }
#endif // BOOST_ASIO_HAS_CO_AWAIT

void
run(
   std::shared_ptr<boost::redis::connection> conn,
   boost::redis::config cfg = {},
   boost::system::error_code ec = boost::asio::error::operation_aborted,
   boost::redis::operation op = boost::redis::operation::receive);

