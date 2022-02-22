/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <map>
#include <vector>
#include <memory>
#include <iostream>
#include <array>
#include <unordered_map>
#include <tuple>
#include <variant>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

namespace net = aedis::net;
namespace redis = aedis::redis;
using aedis::redis::command;
using aedis::redis::client;
using aedis::resp3::node;
using aedis::resp3::response_traits_t;
using client_type = client<aedis::net::ip::tcp::socket>;

namespace aedis {
namespace redis {

template <class Tuple>
class custom_adapter {
private:
   using variant_type =
      boost::mp11::mp_rename<boost::mp11::mp_transform<response_traits_t, Tuple>, std::variant>;

   std::array<variant_type, std::tuple_size<Tuple>::value> adapters_;

public:
   custom_adapter(Tuple* r)
      { resp3::adapter::detail::assigner<std::tuple_size<Tuple>::value - 1>::assign(adapters_, *r); }

   void
   operator()(
      command cmd,
      resp3::type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t size,
      std::error_code& ec)
   {
      int i = -1;
      switch (cmd) {
         case command::lrange: i = 0; break;
         case command::smembers: i = 1; break;
         default: i = 2;
      }

      std::visit([&](auto& arg){arg(t, aggregate_size, depth, data, size, ec);}, adapters_[i]);
   }
};

}
}

class receiver {
private:
   std::shared_ptr<client_type> db_;

   using responses_type =
      std::tuple<
         std::list<int>,
         std::set<std::string>,
         std::vector<node<std::string>>>;

   responses_type resps_;

   std::map<std::string, std::string> map
      { {"key1", "value1"}
      , {"key2", "value2"}
      , {"key3", "value3"}
      };

   std::vector<int> vec
      {1, 2, 3, 4, 5, 6};

   std::set<std::string> set
      {"one", "two", "three", "four"};

public:
   receiver(std::shared_ptr<client_type> db) : db_{db} {}

   auto adapter() { return redis::custom_adapter<responses_type>(&resps_); }

   void on_hello()
   {
      db_->send_range(command::hset, "hset-key", std::cbegin(map), std::cend(map));
      db_->send_range(command::rpush, "rpush-key", std::cbegin(vec), std::cend(vec));
      db_->send_range(command::sadd, "sadd-key", std::cbegin(set), std::cend(set));

      db_->send(command::hgetall, "hset-key");
      db_->send(command::lrange, "rpush-key", 0, -1);
      db_->send(command::smembers, "sadd-key");

      db_->send(command::quit);
   }

   void on_hgetall()
   {
      //for (auto const& e: hgetall3) std::cout << e.first << " ==> " << e.second << "; ";
      std::cout << "\n";
   }

   void on_lrange()
   {
      std::cout << "\n";
      for (auto const& e: std::get<0>(resps_)) std::cout << e << " ";
      std::cout << "\n";
      std::get<0>(resps_).clear();
   }

   void on_smembers()
   {
      std::cout << "\n";
      for (auto const& e: std::get<1>(resps_)) std::cout << e << " ";
      std::cout << "\n";
      std::get<1>(resps_).clear();
   }

   void operator()(command cmd)
   {
      switch (cmd) {
         case command::hello: on_hello(); break;
         case command::hgetall: on_hgetall(); break;
         case command::lrange: on_lrange(); break;
         case command::smembers: on_smembers(); break;
         default:;
      }
   }
};

int main()
{
   net::io_context ioc;
   auto db = std::make_shared<client_type>(ioc.get_executor());

   receiver recv{db};
   db->set_response_adapter(recv.adapter());
   db->set_reader_callback(std::ref(recv));

   db->async_run({net::ip::make_address("127.0.0.1"), 6379}, [](auto){});
   ioc.run();
}
