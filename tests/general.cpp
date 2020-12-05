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

net::awaitable<void> offline()
{
   // Redis answer - Expected vector.
   std::vector<std::pair<std::string, std::vector<std::string>>> payloads
   { {{"+OK\r\n"},                                           {"OK"}}
   , {{":3\r\n"},                                            {"3"}}
   , {{"*3\r\n$3\r\none\r\n$3\r\ntwo\r\n$5\r\nthree\r\n"},   {"one", "two", "three"}}
   , {{"*0\r\n"},                                            {}}
   , {{"$2\r\nhh\r\n"},                                      {"hh"}}
   , {{"$0\r\n"},                                            {""}}
   , {{"-Error\r\n"},                                        {"Error"}}
   , {{",1.23\r\n"},                                         {"1.23"}}
   , {{",inf\r\n"},                                          {"inf"}}
   , {{",-inf\r\n"},                                         {"-inf"}}
   , {{"#f\r\n"},                                            {"f"}}
   , {{"#t\r\n"},                                            {"t"}}
   , {{"!21\r\nSYNTAX invalid syntax\r\n"},                  {"SYNTAX invalid syntax"}}
   , {{"!0\r\n"},                                            {""}}
   , {{"=15\r\ntxt:Some string\r\n"},                        {"txt:Some string"}}
   , {{"(3492890328409238509324850943850943825024385\r\n"},  {"3492890328409238509324850943850943825024385"}}
   , {{"~5\r\n+orange\r\n+apple\r\n#t\r\n:100\r\n:999\r\n"}, {"orange", "apple", "t", "100", "999"}}
   , {{"~0\r\n"},                                            {}}
   , {{"%7\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n6.0.9\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:203\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n"}, {"server", "redis", "version", "6.0.9", "proto", "3", "id", "203", "mode", "standalone", "role", "master", "modules"}}
   , {{"%0\r\n"}, {}}
   , {{"|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n"}, {"key-popularity", "a", "0.1923", "b", "0.0012"}}
   , {{">4\r\n+pubsub\r\n+message\r\n+foo\r\n+bar\r\n"}, {"pubsub", "message", "foo", "bar"}}
   , {{">0\r\n"}, {}}
   , {{"$?\r\n;4\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"}, {"Hell", "o wor", "d"}}
   };

   resp::buffer buffer;
   for (auto const& e : payloads) {
      test_tcp_socket ts {e.first};
      resp::response res;
      co_await resp::async_read(ts, buffer, res);
      if (e.second != res.res) {
        std::cout
	   << "Error: " << std::size(e.second)
	   << " " << std::size(res.res) << std::endl;
      } else {
        std::cout << "Success: Offline tests." << std::endl;
      }
   }
}

int main(int argc, char* argv[])
{
   net::io_context ioc {1};
   co_spawn(ioc, offline(), net::detached);
   co_spawn(ioc, test1(), net::detached);
   //co_spawn(ioc, resp3(), net::detached);
   ioc.run();
}

