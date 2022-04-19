/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <map>
#include <set>
#include <vector>
#include <iostream>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

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

// Helper function to print an aggregate in raw form.
void print_and_clear_aggregate(T0& v)
{
   auto const m = element_multiplicity(v.front().data_type);
   for (auto i = 0lu; i < m * v.front().aggregate_size; ++i)
      std::cout << v[i + 1].value << " ";
   std::cout << "\n";
   v.clear();
}

// Prints an STL container.
void print_and_clear(std::set<std::string>& cont)
{
   for (auto const& e: cont) std::cout << e << " ";
   std::cout << "\n";
   cont.clear();
}

void print_and_clear(std::map<std::string, std::string>& cont)
{
   for (auto const& e: cont)
      std::cout << e.first << ": " << e.second << "\n";
   cont.clear();
}

struct receiver {
public:
   receiver(client_type& db)
   : adapter0_{adapt(resp0_)}
   , adapter1_{adapt(resp1_)}
   , adapter2_{adapt(resp2_)}
   , db_{&db} {}

   void on_connect()
   {
      db_->send(command::hello, 3);
   }

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

   void on_push(std::size_t) { }

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
   receiver recv{db};

   db.async_run(
      recv,
      {net::ip::make_address("127.0.0.1"), 6379},
      [](auto ec){ std::cout << ec.message() << std::endl;});

   ioc.run();
}
