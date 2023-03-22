/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/operation.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/check_health.hpp>
#include <boost/redis/run.hpp>
#include <boost/redis/logger.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/use_future.hpp>
#include <tuple>
#include <string>
#include <chrono>
#include <thread>
#include <iostream>

// Include this in no more than one .cpp file.
#include <boost/redis/src.hpp>

namespace net = boost::asio;
using connection = boost::redis::connection;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore_t;
using boost::redis::async_run;
using boost::redis::address;
using boost::redis::logger;
using boost::redis::async_check_health;
using namespace std::chrono_literals;

template <class Response>
auto exec(std::shared_ptr<connection> conn, request const& req, Response& resp)
{
   net::dispatch(
      conn->get_executor(),
      net::deferred([&]() { return conn->async_exec(req, resp, net::deferred); }))
      (net::use_future).get();
}

auto main(int argc, char * argv[]) -> int
{
   try {
      address addr;

      if (argc == 3) {
         addr.host = argv[1];
         addr.port = argv[2];
      }

      net::io_context ioc{1};

      auto conn = std::make_shared<connection>(ioc);

      // Starts a thread that will can io_context::run on which the
      // connection will run.
      std::thread t{[&ioc, conn, addr]() {
         async_run(*conn, addr, 10s, 10s, logger{}, [conn](auto){
            conn->cancel();
         });

         async_check_health(*conn, "Boost.Redis", 2s, [conn](auto) {
            conn->cancel();
         });

         ioc.run();
      }};

      request req;
      req.push("HELLO", 3);
      req.push("PING");
      req.push("QUIT");

      response<ignore_t, std::string, ignore_t> resp;

      // Executes commands synchronously.
      exec(conn, req, resp);

      std::cout << "Response: " << std::get<1>(resp).value() << std::endl;

      t.join();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
