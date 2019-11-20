/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <deque>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>

#include <aedis/aedis.hpp>

using namespace aedis;

using args_type = std::initializer_list<std::string const>;

void set_get()
{
   net::io_context ioc;
   session ss {ioc};

   auto c = set({"Name", "Marcelo"})
          + get("Name");

   ss.send(std::move(c));

   ss.run();
   ioc.run();
}

void transaction()
{
   net::io_context ioc;
   session ss {ioc};

   auto o = multi()
          + set({"Age", "39"})
          + incr("Age")
          + get("Age")
          + expire("Age", 10)
          + exec();

   ss.send(std::move(o));
   ss.run();
   ioc.run();
}

void rpush_vector()
{
   net::io_context ioc;
   session ss {ioc};

   std::vector<int> v1 {1 , 2, 3, 4, 5, 6, 7};
   ss.send(rpush("a", v1) + lrange("a") + del("a"));

   std::list<std::string> v2 {"one" ,"two", "three"};
   ss.send(rpush("b", v2) + lrange("b") + del("b"));

   std::set<std::string> v3 {"a" ,"b", "c"};
   ss.send(rpush("c", v3) + lrange("c") + del("c"));

   ss.run();
   ioc.run();
}

void pub(int count, char const* channel)
{
   net::io_context ioc;
   session pub_session(ioc);
   for (auto i = 0; i < count; ++i)
      pub_session.send(publish(channel, std::to_string(i)));

   pub_session.run();

   ioc.run();
}

auto msg_handler2 = [i = 0](auto ec, auto const& res) mutable
{
   if (ec) 
      throw std::runtime_error(ec.message());

   auto const n = std::stoi(res.back());
   if (n != i + 1)
      std::cout << "===============> Error." << std::endl;
   std::cout << "Counter: " << n << std::endl;

   i = n;

   //for (auto const& o : res)
   //   std::cout << o << " ";

   //std::cout << std::endl;
};

struct sub_arena {
   session s;

   sub_arena( net::io_context& ioc
            , std::string channel)
   : s(ioc)
   {
      s.set_msg_handler(msg_handler2);

      auto const on_conn_handler = [this, channel]()
         { s.send(subscribe(channel)); };

      s.set_on_conn_handler(on_conn_handler);
      s.run();
   }
};

void sub(char const* channel)
{
   net::io_context ioc;
   sub_arena arena(ioc, channel);
   ioc.run();
}

void zadd()
{
   net::io_context ioc;
   session ss {ioc};

   auto c1 = zadd("foo", 1, "bar1")
           + zadd("foo", 2, "bar2")
           + zadd("foo", 3, "bar3")
           + zadd("foo", 4, "bar4")
           + zadd("foo", 5, "bar5");

   ss.send(c1);

   ss.run();
   ioc.run();
}

void zrangebyscore()
{
   net::io_context ioc;
   session ss(ioc);

   auto c1 = zrangebyscore("foo", 2, -1);

   ss.send(c1);

   ss.run();
   ioc.run();
}

void zrange()
{
   net::io_context ioc;
   session ss(ioc);

   ss.send(zrange("foo", 2, -1));

   ss.run();
   ioc.run();
}

void read_msg_op()
{
   net::io_context ioc;
   session ss(ioc);

   auto c1 = multi()
           + lrange("foo")
           + del("foo")
           + exec();

   ss.send(c1);

   ss.run();
   ioc.run();
}

int main(int argc, char* argv[])
{
   try {
      if (argc == 1) {
         std::cerr << "Usage: " << argv[0] << " n host port" << std::endl;
         return 1;
      }

      session::config cfg;
      auto const n = std::stoi(argv[1]);

      if (argc == 3) {
         cfg.host = argv[2];
         cfg.port = argv[3];
      }

      char const* channel = "foo";

      switch (n) {
         case 0: set_get(); break;
         case 1: transaction(); break;
         case 2: pub(10, channel); break;
         case 3: sub(channel); break;
         case 4: zadd(); break;
         case 5: zrangebyscore(); break;
         case 6: zrange(); break;
         case 7: read_msg_op(); break;
         case 8: rpush_vector(); break;
         default:
            std::cerr << "Option not available." << std::endl;
      }
   } catch (std::exception const& e) {
      std::cerr << e.what() << "\n";
   }

   return 0;
}

