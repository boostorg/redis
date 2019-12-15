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
   s.disable_reconnect();

   s.run();
   ioc.run();
}

void rpush_ex()
{
   std::array<std::string, 3> a
   {"a1", "a2", "a3"};

   std::vector<std::string> b
   {"b1" ,"b2", "b3"};

   std::list<std::string> c
   {"c1" ,"c2", "c3"};

   std::set<std::string> d
   {"d1" ,"d2", "d3"};

   std::deque<std::string> e
   {"e1" ,"e2", "e3"};

   std::forward_list<std::string> f
   {"f1" ,"f2", "f3"};

   std::multiset<std::string> g
   {"g1" ,"g2", "g3"};

   std::unordered_set<std::string> h
   {"h1" ,"h2", "h3"};

   std::unordered_set<std::string> i
   {"i1" ,"i2", "i3"};

   auto s = flushall()
          + role()
          + role()
          + ping()
          + role()
          + ping()
          + role()
          + ping()
          + role()
          + rpush("a", a)
          + lrange("a")
          + rpush("b", b)
          + lrange("b")
          + rpush("c", c)
          + lrange("c")
          + rpush("d", d)
          + lrange("d")
          + rpush("e", e)
          + lrange("e")
          + rpush("f", f)
          + lrange("f")
          + rpush("g", g)
          + lrange("g")
          + rpush("h", h)
          + lrange("h")
          + rpush("i", i)
          + lrange("i")
          + quit()
	  ;

   net::io_context ioc;
   session ss {ioc};

   ss.send(std::move(s));
   ss.disable_reconnect();

   ss.run();
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
          + role()
          + flushall()
          + rpush("a", a)
          + lrange("a")
          + del("a")
          + multi()
          + rpush("b", b)
          + lrange("b")
          + del("b")
          + hset("c", c)
          + hmget("c", {"Name", "Education", "Job"})
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
	  + quit()
	  ;

   net::io_context ioc;
   session ss {ioc};

   ss.send(std::move(s));
   ss.disable_reconnect();

   ss.run();
   ioc.run();
}

void example2()
{
   net::io_context ioc;

   session::config cfg
   { { "127.0.0.1", "26377"
     , "127.0.0.1", "26378"
     , "127.0.0.1", "26379"} // Sentinel addresses
   , "mymaster" // Instance name
   , "master" // Instance role
   , 256 // Max pipeline size
   , log::level::info
   };

   session ss {ioc, cfg, "id"};

   ss.send(role() + quit());
   ss.disable_reconnect();

   ss.run();
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

   s.send(ping() + quit());
   s.disable_reconnect();

   s.run();
   ioc.run();
}

void send_ping()
{
   net::io_context ioc;
   session s {ioc};

   s.send(ping() + quit());
   s.disable_reconnect();

   s.run();
   ioc.run();
}

int main(int argc, char* argv[])
{
   example1();
   example2();
   example3();
   rpush_ex();
   send_ping();
}

