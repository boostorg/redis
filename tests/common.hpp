#pragma once

#include <boost/asio.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT
namespace net = boost::asio;
inline
auto redir(boost::system::error_code& ec)
   { return net::redirect_error(net::use_awaitable, ec); }
#endif // BOOST_ASIO_HAS_CO_AWAIT
