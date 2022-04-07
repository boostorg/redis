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
using aedis::redis::command;
using aedis::generic::client;
using aedis::adapter::adapt;
using aedis::adapter::adapters_t;
using aedis::adapter::make_adapters_tuple;
using client_type = client<net::ip::tcp::socket, command>;
using responses_type =
   std::tuple<
      std::list<int>,
      boost::optional<std::set<std::string>>,
      std::vector<node<std::string>>
   >;
using adapters_type = adapters_t<responses_type>;

template <class Container>
void print_and_clear(Container& cont)
{
   std::cout << "\n";
   for (auto const& e: cont) std::cout << e << " ";
   std::cout << "\n";
   cont.clear();
}

class myreceiver {
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
         case command::lrange:   adapter::get<std::list<int>>(adapters_)(nd, ec);
         case command::smembers: adapter::get<boost::optional<std::set<std::string>>>(adapters_)(nd, ec);
         default:;
      }
   }

   void on_write(std::size_t n)
   { 
      std::cout << "Number of bytes written: " << n << std::endl;
   }

   void on_push() { }

   void on_read(command cmd)
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
         print_and_clear(std::get<std::list<int>>(resps_));
         break;

         case command::smembers:
         print_and_clear(std::get<boost::optional<std::set<std::string>>>(resps_).value());
         break;

         default:;
      }
   }

private:
   responses_type resps_;
   adapters_type adapters_;
   client_type* db_;
};

int main()
{
   net::io_context ioc;
   client_type db{ioc.get_executor()};
   myreceiver recv{db};

   db.async_run(
       recv,
       {net::ip::make_address("127.0.0.1"), 6379},
       [](auto ec){ std::cout << ec.message() << std::endl;});

   ioc.run();
}

