/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "aedis.hpp"

using namespace aedis;

void send(std::string cmd)
{
   net::io_context ioc;
   session s {ioc};

   s.send(std::move(cmd));

   s.run();
   ioc.run();
}

void example1()
{
   std::list<std::string> a
   {"one" ,"two", "three"};

   std::set<std::string> b
   {"a" ,"b", "c"};

   std::map<std::string, std::string> c
   { {{"Name"},      {"Marcelo"}} 
   , {{"Education"}, {"Physics"}}
   , {{"Job"},       {"Programmer"}}};

   std::map<int, std::string> d
   { {1, {"foo"}} 
   , {2, {"bar"}}
   , {3, {"foobar"}}
   };

   auto s = ping()
          + flushall()
          + rpush("a", a)
          + lrange("a")
          + del("a")
          + multi()
          + rpush("b", b)
          + lrange("b")
          + del("b")
          + hset("c", c)
          + hvals("c")
          + zadd({"d"}, d)
          + zrange("d")
          + zrangebyscore("foo", 2, -1)
          + set("f", {"39"})
          + incr("f")
          + get("f")
          + expire("f", 10)
          + publish("g", "A message")
          + exec()
	  + set("h", {"h"})
	  + append("h", "h")
	  + get("h")
	  + auth("password")
	  + bitcount("h")
	  ;

  send(std::move(s));
}

void example2()
{
   net::io_context ioc;

   session::config cfg
   { "127.0.0.1" // host
   , "6379" // port
   , 256 // Max pipeline size
   , std::chrono::milliseconds {500} // Connection retry
   , {} // Sentinel addresses
   , log::level::info
   };

   session s {ioc, cfg, "id"};

   s.send(ping());

   s.run();
   ioc.run();
}

void example3()
{
   net::io_context ioc;
   session s {ioc};

   s.set_on_conn_handler([]() {
      std::cout << "Connected" << std::endl;
   });

   s.set_msg_handler([](auto ec, auto res) {
      if (ec) {
         std::cerr << "Error: " << ec.message() << std::endl;
      }

      std::copy( std::cbegin(res)
               , std::cend(res)
               , std::ostream_iterator<std::string>(std::cout, " "));

      std::cout << std::endl;
   });

   s.send(ping());

   s.run();
   ioc.run();
}

int main(int argc, char* argv[])
{
   example1();
   //example2();
   //example3();
}

