/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <tuple>
#include <string>
#include <chrono>
#include <thread>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/redis.hpp>
#include <boost/redis/check_health.hpp>

// Include this in no more than one .cpp file.
#include <boost/redis/src.hpp>

namespace net = boost::asio;
using connection = boost::redis::connection;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore_t;
using boost::redis::async_run;
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
      std::string host = "127.0.0.1";
      std::string port = "6379";

      if (argc == 3) {
         host = argv[1];
         port = argv[2];
      }

      net::io_context ioc{1};

      auto conn = std::make_shared<connection>(ioc);

      // Starts a thread that will can io_context::run on which the
      // connection will run.
      std::thread t{[&ioc, conn, host, port]() {
         async_run(*conn, host, port, 10s, 10s, [conn](auto){
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
