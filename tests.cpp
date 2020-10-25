/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "aedis.hpp"

using namespace aedis;

void check_size(
   std::vector<std::string> const& v,
   unsigned s,
   std::string msg)
{
   if (std::size(v) != s)
      throw std::runtime_error(msg);

   msg += "size ok.";
   std::cout << msg << std::endl;
}

void check_string(
   std::string const& a,
   std::string const& b,
   std::string msg)
{
   if (a != b)
      throw std::runtime_error(msg);

   msg += "string ok.";
   std::cout << msg << std::endl;
}

template <class Iter1, class Iter2>
void check_equal(
   Iter1 begin1,
   Iter1 end1,
   Iter2 begin2,
   std::string msg)
{
   auto const r =
      std::equal( begin1
                , end1
                , begin2);

   if (!r)
      throw std::runtime_error(msg);

   msg += "equal ok.";
   std::cout << msg << std::endl;
}

void rpush_lrange()
{
   session<tcp::socket>::config cfg
   { {"127.0.0.1", "26379"}
   , "mymaster"
   , "master"
   , 4
   , log::level::info
   };

   net::io_context ioc;
   session<tcp::socket> ss {ioc, cfg};

   std::array<std::string, 3> a
   {"a1", "a2", "a3"};

   auto s = flushall()
          + rpush("a", a)
          + lrange("a")
          + ping();

   auto const cycle = 4;
   auto const repeat = 32;

   for (auto i = 0; i < repeat; ++i)
      ss.send(s);

   auto const size = repeat * cycle;
   auto handler = [&, size, i = 0](auto ec, auto res) mutable
   {
      if (ec) {
         std::cerr << "Error: " << ec.message() << std::endl;
         return;
      }

      std::string const prefix = "Test: ";

      auto const f = i % cycle;
      switch (f) {
      case 0:
      {
         check_size(res, 1, prefix);
         check_string(res.front(), "OK", prefix);
      } break;
      case 1:
      {
         check_size(res, 1, prefix);
         check_string(res.front(), "3", prefix);
      } break;
      case 2:
      {
         check_equal( std::cbegin(res)
                    , std::cend(res)
                    , std::cbegin(a)
                    , prefix);
      } break;
      case 3:
      {
         check_size(res, 1, prefix);
         check_string(res.front(), "PONG", prefix);

         if (i == size - 1) {
            ss.send(quit());
            ss.disable_reconnect();
         }
      }
      }
      ++i;
   };

   ss.set_msg_handler(handler);

   ss.run();
   ioc.run();
}

int main(int argc, char* argv[])
{
   rpush_lrange();
}

