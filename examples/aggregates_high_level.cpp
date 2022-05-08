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
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "print.hpp"

namespace net = boost::asio;
using aedis::adapter::adapt;
using aedis::adapter::adapter_t;
using aedis::redis::command;
using net::experimental::as_tuple;
using client_type = aedis::generic::client<net::ip::tcp::socket, command>;
using node_type = aedis::resp3::node<boost::string_view>;
using error_code = boost::system::error_code;
using namespace net::experimental::awaitable_operators;

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
         case command::hgetall: adapter2_(nd, ec); break;
         default:;
      }
   }

private:
   adapter_t<T0> adapter0_;
   adapter_t<T1> adapter1_;
   adapter_t<T2> adapter2_;
};

net::awaitable<void>
reader(std::shared_ptr<client_type> db)
{
   T0 resp0;
   T1 resp1;
   T2 resp2;

   db->set_adapter(adapter{resp0, resp1, resp2});

   for (;;) {
      auto [ec, cmd, n] = co_await db->async_receive(as_tuple(net::use_awaitable));
      if (ec)
         co_return;

      std::cout << "on_read: " << cmd << ", " << n << "\n";

      switch (cmd) {
         case command::hello:
         {
            db->send_range(command::rpush, "rpush-key", vec);
            db->send_range(command::sadd, "sadd-key", set);
            db->send_range(command::hset, "hset-key", map);
         } break;

         case command::rpush:
         db->send(command::lrange, "rpush-key", 0, -1);
         break;

         case command::sadd:
         db->send(command::smembers, "sadd-key");
         break;

         case command::hset:
         db->send(command::hgetall, "hset-key");
         db->send(command::quit);
         break;

         case command::lrange:
         print_and_clear_aggregate(resp0);
         break;

         case command::smembers:
         print_and_clear(resp1);
         break;

         case command::hgetall:
         print_and_clear(resp2);
         break;

         default:;
      }
   }
}

net::awaitable<void> run()
{
   auto ex = co_await net::this_coro::executor;
   auto db = std::make_shared<client_type>(ex);
   co_await (db->async_run(net::use_awaitable) && reader(db));
}

int main()
{
   net::io_context ioc;
   net::co_spawn(ioc.get_executor(), run(), net::detached);
   ioc.run();
}
