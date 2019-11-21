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
   session ss {ioc};

   ss.send(std::move(cmd));

   ss.run();
   ioc.run();
}

int main(int argc, char* argv[])
{
   std::list<std::string> b
   {"one" ,"two", "three"};

   std::set<std::string> c
   {"a" ,"b", "c"};

   std::map<std::string, std::string> d
   { {{"Name"},      {"Marcelo"}} 
   , {{"Education"}, {"Physics"}}
   , {{"Job"},       {"Programmer"}}};

   std::map<int, std::string> e
   { {1, {"foo"}} 
   , {2, {"bar"}}
   , {3, {"foobar"}}
   };

   auto s = ping()
          + rpush("b", b)
          + lrange("b")
          + del("b")
          + multi()
          + rpush("c", c)
          + lrange("c")
          + del("c")
          + hset("d", d)
          + hvals("d")
          + zadd({"e"}, e)
          + zrange("e")
          + zrangebyscore("foo", 2, -1)
          + set("f", {"39"})
          + incr("f")
          + get("f")
          + expire("f", 10)
          + publish("g", "A message")
          + exec();

  send(std::move(s));
}

