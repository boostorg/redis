/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_LOGGER_HPP
#define BOOST_REDIS_LOGGER_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>

namespace boost::redis {

// TODO: Move to ipp file.
// TODO: Implement filter.
class logger {
public:
   void on_resolve(system::error_code const& ec, asio::ip::tcp::resolver::results_type const&)
   {
      // TODO: Print the endpoints
      std::clog << "on_resolve: " << ec.message() << std::endl;
   }

   void on_connect(system::error_code const& ec, asio::ip::tcp::endpoint const&)
   {
      // TODO: Print the endpoint
      std::clog << "on_connect: " << ec.message() << std::endl;
   }

   void on_connection_lost()
   {
      std::clog << "on_connection_lost: " << std::endl;
   }

   void on_hello(system::error_code const& ec)
   {
      std::clog << "on_hello: " << ec.message() << std::endl;
   }
};

} // boost::redis

#endif // BOOST_REDIS_LOGGER_HPP
