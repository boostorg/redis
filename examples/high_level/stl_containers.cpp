/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <map>
#include <vector>
#include <iostream>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = boost::asio;
namespace adapter = aedis::adapter;
using aedis::resp3::node;
using aedis::adapter::adapt;
using aedis::adapter::adapter_t;
using aedis::redis::command;
using aedis::generic::client;

// Responses used in the example.
using T0 = std::list<int>;
using T1 = boost::optional<std::set<std::string>>;

using client_type = client<net::ip::tcp::socket, command>;

template <class Container>
void print_and_clear(Container& cont)
{
   std::cout << "\n";
   for (auto const& e: cont) std::cout << e << " ";
   std::cout << "\n";
   cont.clear();
}

class receiver {
public:
   receiver(client_type& db)
   : adapter0_(adapt(resp0_))
   , adapter1_(adapt(resp1_))
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
         case command::lrange:   adapter0_(nd, ec); break;
         case command::smembers: adapter1_(nd, ec); break;
         default:;
      }
   }

   void on_read(command cmd, std::size_t)
   {
      switch (cmd) {
         case command::hello:
         {
            std::map<std::string, std::string> map
               { {"key1", "value1"}
               , {"key2", "value2"}
               , {"key3", "value3"}
               };

            std::vector<int> vec
               {1, 2, 3, 4, 5, 6};

            std::set<std::string> set
               {"one", "two", "three", "four"};

            // Sends the stl containers.
            db_->send_range(command::hset, "hset-key", map);
            db_->send_range(command::rpush, "rpush-key", vec);
            db_->send_range(command::sadd, "sadd-key", set);

            //_ Retrieves the containers.
            db_->send(command::hgetall, "hset-key");
            db_->send(command::lrange, "rpush-key", 0, -1);
            db_->send(command::smembers, "sadd-key");
            db_->send(command::quit);
         } break;

         case command::lrange:
         print_and_clear(resp0_);
         break;

         case command::smembers:
         print_and_clear(resp1_.value());
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
   // Responses
   T0 resp0_;
   T1 resp1_;

   // Adapters.
   adapter_t<T0> adapter0_;
   adapter_t<T1> adapter1_;

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

