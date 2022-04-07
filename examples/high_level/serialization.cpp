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
using aedis::redis::command;
using aedis::adapter::adapters_t;
using aedis::adapter::make_adapters_tuple;
using aedis::adapter::get;
using aedis::generic::client;
using client_type = client<net::ip::tcp::socket, command>;
using responses_type =
   std::tuple<
      boost::optional<mystruct>, // get
      std::list<mystruct>, // lrange
      std::set<mystruct>, // smembers
      std::map<std::string, mystruct>  // hgetall
   >;
using adapters_type = adapters_t<responses_type>;

std::ostream& operator<<(std::ostream& os, mystruct const& obj)
{
   os << "a: " << obj.a << ", b: " << obj.b;
   return os;
}

bool operator<(mystruct const& a, mystruct const& b)
{
   return std::tie(a.a, a.b) < std::tie(b.a, b.b);
}

// Dumy serialization.
void to_bulk(std::string& to, mystruct const& obj)
{
   aedis::resp3::to_bulk(to, "Dummy serializaiton string.");
}

// Dummy deserialization.
void from_string(mystruct& obj, boost::string_view sv, boost::system::error_code& ec)
{
   obj.a = 1;
   obj.b = 2;
}

class myreceiver  {
private:
   responses_type resps_;
   adapters_type adapters_;
   client_type* db_;

public:
   myreceiver(client_type& db)
   : adapters_(make_adapters_tuple(resps_))
   , db_{&db} {}

   void
   on_resp3(
      command cmd,
      node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      switch (cmd) {
         case command::get:      adapter::get<boost::optional<mystruct>>(adapters_)(nd, ec);
         case command::lrange:   adapter::get<std::list<mystruct>>(adapters_)(nd, ec);
         case command::smembers: adapter::get<std::set<mystruct>>(adapters_)(nd, ec);
         case command::hgetall:  adapter::get<std::map<std::string, mystruct>>(adapters_)(nd, ec);
         default:; // Ignore
      }
   }

   void on_read(command cmd)
   {
      std::cout << cmd << "\n";

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
            if (std::get<boost::optional<mystruct>>(resps_).has_value()) {
               std::cout << std::get<boost::optional<mystruct>>(resps_).value() << "\n\n";
               std::get<boost::optional<mystruct>>(resps_).reset();
            } else {
               std::cout << "Expired." << "\n";
            }
         } break;

         case command::lrange:
         for (auto const& e: std::get<std::list<mystruct>>(resps_))
            std::cout << e << "\n";
         std::cout << "\n";
         std::get<std::list<mystruct>>(resps_).clear();
         break;

         case command::smembers:
         for (auto const& e: std::get<std::set<mystruct>>(resps_))
            std::cout << e << "\n";
         std::cout << "\n";
         std::get<std::set<mystruct>>(resps_).clear();
         break;

         case command::hgetall:
         for (auto const& e: std::get<std::map<std::string, mystruct>>(resps_))
            std::cout << e.first << ", " << e.second << std::endl;
         std::cout << "\n";
         std::get<std::map<std::string, mystruct>>(resps_).clear();
         break;

         default:;
      }
   }

   void on_write(std::size_t n) { }
   void on_push() { }
};

int main()
{
   net::io_context ioc;
   client_type db(ioc.get_executor());
   myreceiver recv{db};

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
