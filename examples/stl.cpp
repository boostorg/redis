/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <map>
#include <set>
#include <vector>
#include <iostream>

#include <boost/asio.hpp>
#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "print.hpp"

namespace net = boost::asio;
using aedis::adapter::adapt;
using aedis::adapter::adapter_t;
using aedis::redis::command;
using aedis::generic::request;
using connection = aedis::generic::connection<command>;
using node_type = aedis::resp3::node<boost::string_view>;
using error_code = boost::system::error_code;

// Response types we use in this example.
using T0 = std::vector<aedis::resp3::node<std::string>>;
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

struct adapter {
public:
   adapter(T0& resp0, T1& resp1, T2& resp2)
   : adapter0_{adapt(resp0)}
   , adapter1_{adapt(resp1)}
   , adapter2_{adapt(resp2)}
   { }

   void operator()(command cmd, node_type const& nd, error_code& ec)
   {
      switch (cmd) {
         case command::lrange:   adapter0_(nd, ec); break;
         case command::smembers: adapter1_(nd, ec); break;
         case command::hgetall:  adapter2_(nd, ec); break;
         default:;
      }
   }

private:
   adapter_t<T0> adapter0_;
   adapter_t<T1> adapter1_;
   adapter_t<T2> adapter2_;
};


auto on_run =[](auto ec)
{
   std::printf("Run: %s\n", ec.message().data());
};

auto on_exec = [](auto ec, std::size_t read_size)
{
   std::printf("Exec: %s %lu\n", ec.message().data(), read_size);
};

int main()
{
   T0 resp0;
   T1 resp1;
   T2 resp2;

   net::io_context ioc;
   connection db{ioc.get_executor(), adapter{resp0, resp1, resp2}};

   request<command> req;
   req.push_range(command::rpush, "rpush-key", vec);
   req.push_range(command::sadd, "sadd-key", set);
   req.push_range(command::hset, "hset-key", map);
   req.push(command::lrange, "rpush-key", 0, -1);
   req.push(command::smembers, "sadd-key");
   req.push(command::hgetall, "hset-key");
   req.push(command::quit);
   db.async_exec(req, on_exec);

   db.async_run(on_run);
   ioc.run();

   print_and_clear_aggregate(resp0);
   print_and_clear(resp1);
   print_and_clear(resp2);
}
