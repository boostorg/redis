/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/aedis.hpp>

#include "test_stream.hpp"

namespace net = aedis::net;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;
using test_tcp_socket = net::use_awaitable_t<>::as_default_on_t<aedis::test_stream<aedis::net::system_executor>>;

namespace this_coro = net::this_coro;

using namespace aedis;

void check_equal(
   std::vector<std::string> const& a,
   std::vector<std::string> const& b)
{
   auto const r =
      std::equal( std::cbegin(a)
                , std::cend(a)
                , std::cbegin(b));

   if (r)
     std::cout << "Success" << std::endl;
   else
     std::cout << "Error" << std::endl;
}

net::awaitable<void> test1()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const rr = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, rr);

   std::vector<std::vector<std::string>> r;
   resp::pipeline p;

   p.flushall();
   r.push_back({"OK"});

   p.ping();
   r.push_back({"PONG"});

   p.rpush("a", std::list<std::string>{"1" ,"2", "3"});
   r.push_back({"3"});

   p.rpush("a", std::vector<std::string>{"4" ,"5", "6"});
   r.push_back({"6"});

   p.rpush("a", std::set<std::string>{"7" ,"8", "9"});
   r.push_back({"9"});

   p.rpush("a", std::initializer_list<std::string>{"10" ,"11", "12"});
   r.push_back({"12"});

   p.lrange("a");
   r.push_back({"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"});

   p.lrange("a", 4, -5);
   r.push_back({"5", "6", "7", "8"});

   p.ltrim("a", 4, -5);
   r.push_back({"OK"});

   p.lpop("a");
   r.push_back({"5"});

   p.lpop("a");
   r.push_back({"6"});

   p.quit();
   r.push_back({"OK"});

   co_await async_write(socket, net::buffer(p.payload));

   resp::buffer buffer;
   for (auto const& o : r) {
     resp::response res;
     co_await resp::async_read(socket, buffer, res);
     check_equal(res.res, o);
   }
}

net::awaitable<void> resp3()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const rr = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, rr);

   std::vector<std::vector<std::string>> r;
   resp::pipeline p;

   p.hello("3");
   r.push_back({"OK"});

   p.quit();
   r.push_back({"OK"});

   co_await async_write(socket, net::buffer(p.payload));

   resp::buffer buffer;
   for (auto const& o : r) {
     resp::response res;
     co_await resp::async_read(socket, buffer, res);
     resp::print(res.res);
     check_equal(res.res, o);
   }
}

struct test_handler {
   void operator()(boost::system::error_code ec) const
   {
      if (ec)
         std::cout << ec.message() << std::endl;
   }
};

void send(std::string cmd)
{
   net::io_context ioc;
   session<tcp::socket> s {ioc};

   s.send(std::move(cmd));
   s.disable_reconnect();

   s.run();
   ioc.run();
}

net::awaitable<void> offline()
{
   // Redis answer - Expected vector.
   std::vector<std::pair<std::string, std::vector<std::string>>> payloads
   { {{"+OK\r\n"},                                         {"OK"}}
   , {{":3\r\n"},                                          {"3"}}
   , {{"*3\r\n$3\r\none\r\n$3\r\ntwo\r\n$5\r\nthree\r\n"}, {"one", "two", "three"}}
   , {{"$2\r\nhh\r\n"},                                    {"hh"}}
   , {{"-Error\r\n"},                                      {"Error"}}
   };

   resp::buffer buffer;
   for (auto const& e : payloads) {
      test_tcp_socket ts {e.first};
      resp::response res;
      co_await resp::async_read(ts, buffer, res);
      if (e.second != res.res)
        std::cout << "Error" << std::endl;
      else
        std::cout << "Success: Offline tests." << std::endl;
   }
}

int main(int argc, char* argv[])
{
   //send(ping());
   net::io_context ioc {1};
   co_spawn(ioc, offline(), net::detached);
   co_spawn(ioc, test1(), net::detached);
   //co_spawn(ioc, resp3(), detached);
   ioc.run();
}

