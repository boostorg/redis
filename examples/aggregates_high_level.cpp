/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <map>
#include <set>
#include <vector>
#include <iostream>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "print.hpp"

namespace net = boost::asio;
using aedis::resp3::node;
using aedis::adapter::adapt;
using aedis::adapter::adapter_t;
using aedis::redis::command;
using aedis::generic::client;

using client_type = client<net::ip::tcp::socket, command>;

// Response types we use in this example.
using T0 = std::vector<node<std::string>>;
using T1 = std::set<std::string>;
using T2 = std::map<std::string, std::string>;

// Some containers we will store in Redis as example.
std::vector<int> vec
   {1, 2, 3, 4, 5, 6};

std::set<std::string> set
   {"one", "two", "three", "four"};

std::map<std::string, std::string> map
   { {"key1", "value1"}
   , {"key2", "value2"}
   , {"key3", "value3"}
   };

struct receiver {
public:
   receiver(client_type& db)
   : adapter0_{adapt(resp0_)}
   , adapter1_{adapt(resp1_)}
   , adapter2_{adapt(resp2_)}
   , db_{&db} {}

   void on_resp3(command cmd, node<boost::string_view> const& nd, boost::system::error_code& ec)
   {
      switch (cmd) {
         case command::lrange:   adapter0_(nd, ec); break;
         case command::smembers: adapter1_(nd, ec); break;
         case command::hgetall: adapter2_(nd, ec); break;
         default:;
      }
   }

   void on_read(command cmd, std::size_t n)
   {
      std::cout << "on_read: " << cmd << ", " << n << "\n";

      switch (cmd) {
         case command::hello:
         {
            db_->send_range(command::rpush, "rpush-key", vec);
            db_->send_range(command::sadd, "sadd-key", set);
            db_->send_range(command::hset, "hset-key", map);
         } break;

         case command::rpush:
         db_->send(command::lrange, "rpush-key", 0, -1);
         break;

         case command::sadd:
         db_->send(command::smembers, "sadd-key");
         break;

         case command::hset:
         db_->send(command::hgetall, "hset-key");
         db_->send(command::quit);
         break;

         case command::lrange:
         print_and_clear_aggregate(resp0_);
         break;

         case command::smembers:
         print_and_clear(resp1_);
         break;

         case command::hgetall:
         print_and_clear(resp2_);
         break;

         default:;
      }
   }

   void on_write(std::size_t n)
   { 
      std::cout << "on_write: " << n << std::endl;
   }

   void on_push(std::size_t n) { }

private:
   T0 resp0_;
   T1 resp1_;
   T2 resp2_;
   adapter_t<T0> adapter0_;
   adapter_t<T1> adapter1_;
   adapter_t<T2> adapter2_;
   client_type* db_;
};

int main()
{
   net::io_context ioc;
   client_type db{ioc.get_executor()};

   auto recv = std::make_shared<receiver>(db);
   db.set_receiver(recv);
   db.async_run([](auto ec){ std::cout << ec.message() << std::endl;});
   ioc.run();
}
