/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#define BOOST_TEST_MODULE low level
#include <boost/test/included/unit_test.hpp>

#include <boost/redis.hpp>
#include <boost/redis/ssl/connection.hpp>
#include <boost/redis/src.hpp>
#include "common.hpp"

namespace net = boost::asio;

using boost::redis::adapt;
using connection = boost::redis::ssl::connection;
using boost::redis::request;
using boost::redis::response;

struct endpoint {
   std::string host;
   std::string port;
};

bool verify_certificate(bool, net::ssl::verify_context&)
{
   std::cout << "set_verify_callback" << std::endl;
   return true;
}

BOOST_AUTO_TEST_CASE(ping)
{
   std::string const in = "Kabuf";

   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3, "AUTH", "aedis", "aedis");
   req.push("PING", in);
   req.push("QUIT");

   std::string out;
   auto resp = std::tie(std::ignore, out, std::ignore);

   auto const endpoints = resolve("db.occase.de", "6380");

   net::io_context ioc;
   net::ssl::context ctx{net::ssl::context::sslv23};
   connection conn{ioc, ctx};
   conn.next_layer().set_verify_mode(net::ssl::verify_peer);
   conn.next_layer().set_verify_callback(verify_certificate);

   net::connect(conn.lowest_layer(), endpoints);
   conn.next_layer().handshake(net::ssl::stream_base::client);

   conn.async_exec(req, adapt(resp), [](auto ec, auto) {
      BOOST_TEST(!ec);
   });

   conn.async_run([](auto ec) {
      BOOST_TEST(!ec);
   });

   ioc.run();

   BOOST_CHECK_EQUAL(in, out);
}

