//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_TRANSPORT_TYPE_HPP
#define BOOST_REDIS_TRANSPORT_TYPE_HPP

namespace boost::redis::detail {

// What transport are we using?
enum class transport_type
{
   tcp,          // plaintext TCP
   tcp_tls,      // TLS over TCP
   unix_socket,  // UNIX domain sockets
};

}  // namespace boost::redis::detail

#endif
