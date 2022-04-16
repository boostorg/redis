/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <iostream>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

// Arbitrary struct to de/serialize.
struct mystruct {
  int a;
  int b;
};

namespace net = boost::asio;
namespace adapter = aedis::adapter;
using aedis::resp3::node;
using aedis::adapter::adapt;
using aedis::adapter::adapter_t;
using aedis::redis::command;
using aedis::generic::client;

using client_type = client<net::ip::tcp::socket, command>;
// Response types used in the example.
using T0 = boost::optional<mystruct>;
using T1 = std::list<mystruct>;
using T2 = std::set<mystruct>;
using T3 = std::map<std::string, mystruct>;

std::ostream& operator<<(std::ostream& os, mystruct const& obj)
{
   os << "a: " << obj.a << ", b: " << obj.b;
   return os;
}

bool operator<(mystruct const& a, mystruct const& b)
{
   return std::tie(a.a, a.b) < std::tie(b.a, b.b);
}

// Dummy serialization.
void to_bulk(std::string& to, mystruct const& obj)
{
   std::string const str = "Dummy serializaiton string.";
   aedis::resp3::add_header(to, aedis::resp3::type::blob_string, str.size());
   aedis::resp3::add_blob(to, str);
}

// Dummy deserialization.
void from_string(mystruct& obj, boost::string_view sv, boost::system::error_code& ec)
{
   obj.a = 1;
   obj.b = 2;
}

class receiver  {
public:
   receiver(client_type& db)
   : adapter0_(adapt(resp0_))
   , adapter1_(adapt(resp1_))
   , adapter2_(adapt(resp2_))
   , adapter3_(adapt(resp3_))
   , db_{&db} {}

   void on_connect()
   {
      db_->send(command::hello, 3);
   }

   void
   on_resp3(
      command cmd,
      node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      switch (cmd) {
         case command::get:      adapter0_(nd, ec); break;
         case command::lrange:   adapter1_(nd, ec); break;
         case command::smembers: adapter2_(nd, ec); break;
         case command::hgetall:  adapter3_(nd, ec); break;
         default:; // Ignore
      }
   }

   void on_read(command cmd, std::size_t n)
   {
      std::cout << cmd << ": " << n << "\n";

      switch (cmd) {
         case command::hello:
         {
            mystruct var{1, 2};

            std::map<std::string, mystruct> map
               { {"key1", {1, 2}}
               , {"key2", {3, 4}}
               , {"key3", {5, 6}}};

            std::vector<mystruct> vec
               {{1, 2}, {3, 4}, {5, 6}};

            std::set<std::string> set
               {{1, 2}, {3, 4}, {5, 6}};

            // Sends
            db_->send(command::set, "serialization-var-key", var, "EX", "2");
            db_->send_range(command::hset, "serialization-hset-key", map);
            db_->send_range(command::rpush, "serialization-rpush-key", vec);
            db_->send_range(command::sadd, "serialization-sadd-key", set);

            // Retrieves
            db_->send(command::get, "serialization-var-key");
            db_->send(command::hgetall, "serialization-hset-key");
            db_->send(command::lrange, "serialization-rpush-key", 0, -1);
            db_->send(command::smembers, "serialization-sadd-key");
         } break;

         case command::get:
         {
            if (resp0_.has_value()) {
               std::cout << resp0_.value() << "\n\n";
               resp0_.reset();
            } else {
               std::cout << "Expired." << "\n";
            }
         } break;

         case command::lrange:
         for (auto const& e: resp1_)
            std::cout << e << "\n";
         std::cout << "\n";
         resp1_.clear();
         break;

         case command::smembers:
         for (auto const& e: resp2_)
            std::cout << e << "\n";
         std::cout << "\n";
         resp2_.clear();
         break;

         case command::hgetall:
         for (auto const& e: resp3_)
            std::cout << e.first << ", " << e.second << std::endl;
         std::cout << "\n";
         resp3_.clear();
         break;

         default:;
      }
   }

   void on_write(std::size_t n)
   { 
      std::cout << "Number of bytes written: " << n << std::endl;
   }

   void on_push() { }

private:
   T0 resp0_;
   T1 resp1_;
   T2 resp2_;
   T3 resp3_;

   adapter_t<T0> adapter0_;
   adapter_t<T1> adapter1_;
   adapter_t<T2> adapter2_;
   adapter_t<T3> adapter3_;

   client_type* db_;
};

int main()
{
   net::io_context ioc;
   client_type db(ioc.get_executor());
   receiver recv{db};

   db.async_run(
      recv,
      {net::ip::make_address("127.0.0.1"), 6379},
      [](auto ec){ std::cout << ec.message() << std::endl;});

   net::steady_timer tm{ioc, std::chrono::seconds{3}};

   tm.async_wait([&db](auto ec){
       db.send(command::get, "serialization-var-key");
       db.send(command::quit);
   });

   ioc.run();
}
