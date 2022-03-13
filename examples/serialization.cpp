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

namespace net = aedis::net;
using aedis::redis::command;
using aedis::redis::receiver_base;
using client_type = aedis::redis::client<aedis::net::ip::tcp::socket>;

// Arbitrary struct to de/serialize.
struct mystruct {
  int a;
  int b;
};

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
std::string to_string(mystruct const& obj)
{
   return "Dummy serializaiton string.";
}

// Dummy deserialization.
void from_string(mystruct& obj, char const* data, std::size_t size, std::error_code& ec)
{
   obj.a = 1;
   obj.b = 2;
}

using transaction_type =
   std::tuple<
      mystruct,
      std::vector<mystruct>,
      std::map<std::string, mystruct>
   >;

// One tuple element for each expected request.
using receiver_type =
   receiver_base<
      std::optional<mystruct>, // get
      std::list<mystruct>, // lrange
      std::set<mystruct>, // smembers
      std::map<std::string, mystruct>,  // hgetall
      transaction_type // exec
   >;

struct myreceiver : receiver_type {
public:
   myreceiver(client_type& db) : db_{&db} {}

private:
   client_type* db_;

   int to_tuple_idx_impl(command cmd) override
   {
      switch (cmd) {
         case command::get:      return index_of<std::optional<mystruct>>();
         case command::lrange:   return index_of<std::list<mystruct>>();
         case command::smembers: return index_of<std::set<mystruct>>();
         case command::hgetall:  return index_of<std::map<std::string, mystruct>>();
         case command::exec:     return index_of<transaction_type>();
         default: return -1;
      }
   }

   void on_read_impl(command cmd) override
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

            // Transaction
            db_->send(command::multi);
            db_->send(command::get, "serialization-var-key");
            db_->send(command::lrange, "serialization-rpush-key", 0, -1);
            db_->send(command::hgetall, "serialization-hset-key");
            db_->send(command::exec);
         } break;

         case command::get:
         {
            if (get<std::optional<mystruct>>().has_value()) {
               std::cout << get<std::optional<mystruct>>().value() << "\n\n";
               get<std::optional<mystruct>>().reset();
            } else {
               std::cout << "Expired." << "\n";
            }
         } break;

         case command::lrange:
         for (auto const& e: get<std::list<mystruct>>())
            std::cout << e << "\n";
         std::cout << "\n";
         get<std::list<mystruct>>().clear();
         break;

         case command::smembers:
         for (auto const& e: get<std::set<mystruct>>())
            std::cout << e << "\n";
         std::cout << "\n";
         get<std::set<mystruct>>().clear();
         break;

         case command::hgetall:
         for (auto const& e: get<std::map<std::string, mystruct>>())
            std::cout << e.first << ", " << e.second << std::endl;
         std::cout << "\n";
         get<std::map<std::string, mystruct>>().clear();
         break;

         case command::exec:
         {
            std::cout
               << "First element: \n"
               << std::get<mystruct>(get<transaction_type>()) << "\n";

            std::cout << "Second element: \n";
            for (auto const& e: std::get<std::vector<mystruct>>(get<transaction_type>()))
               std::cout << e << "\n";
            std::cout << "\n";
            std::get<std::vector<mystruct>>(get<transaction_type>()).clear();
         } break;

         default:;
      }
   }
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
