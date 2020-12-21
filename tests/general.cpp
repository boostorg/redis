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

template <class T>
void check_equal(T const& a, T const& b, std::string const& msg = "")
{
   if (a == b)
     std::cout << "Success: " << msg << std::endl;
   else
     std::cout << "Error: " << msg << std::endl;
}

net::awaitable<void> test_list()
{
   std::list<int> list {1 ,2, 3, 4, 5, 6};

   resp::pipeline p;
   p.hello("3");
   p.flushall();
   p.rpush("a", list);
   p.lrange("a");
   p.lrange("a", 2, -2);
   p.ltrim("a", 2, -2);
   p.lpop("a");
   p.quit();

   auto ex = co_await this_coro::executor;
   tcp::resolver resv(ex);
   auto const rr = resv.resolve("127.0.0.1", "6379");
   tcp_socket socket {ex};
   co_await async_connect(socket, rr);
   co_await async_write(socket, net::buffer(p.payload));
   std::string buffer;

   {  // hello
      resp::response res;
      co_await resp::async_read(socket, buffer, res);
   }

   {  // flushall
      resp::response_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, std::string {"OK"}, "flushall");
   }

   {  // rpush
      resp::response_number<int> res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, 6, "rpush");
   }

   {  // lrange
      resp::response_list<int> res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, list, "lrange-1");
   }

   {  // lrange
      resp::response_list<int> res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, std::list<int>{3, 4, 5}, "lrange-2");
   }

   {  // ltrim
      resp::response_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, std::string {"OK"}, "ltrim");
   }

   {  // lpop. Why a blob string instead of a number?
      resp::response_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, std::string{"3"}, "lpop");
   }

   {  // quit
      resp::response_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, std::string {"OK"}, "quit");
   }
}

net::awaitable<void> test_set()
{
   // Tests whether the parser can handle payloads that contain the separator.
   std::string test_bulk1(10000, 'a');
   test_bulk1[30] = '\r';
   test_bulk1[31] = '\n';

   std::string test_bulk2 = "aaaaa";

   auto ex = co_await this_coro::executor;
   tcp::resolver resv(ex);
   auto const rr = resv.resolve("127.0.0.1", "6379");
   tcp_socket socket {ex};
   co_await async_connect(socket, rr);

   resp::pipeline p;
   p.hello("3");
   p.flushall();
   p.set("s", {test_bulk1});
   p.get("s");
   p.set("s", {test_bulk2});
   p.get("s");
   p.set("s", {""});
   p.get("s");
   p.quit();

   co_await async_write(socket, net::buffer(p.payload));

   std::string buffer;
   {  // hello, flushall
      resp::response res;
      co_await resp::async_read(socket, buffer, res);
      co_await resp::async_read(socket, buffer, res);
   }

   {  // set
      resp::response_simple_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, std::string {"OK"}, "set1");
   }

   {  // get
      resp::response_blob_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, test_bulk1, "get1");
   }

   {  // set
      resp::response_simple_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, std::string {"OK"}, "set1");
   }

   {  // get
      resp::response_blob_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, test_bulk2, "get2");
   }

   {  // set
      resp::response_simple_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, std::string {"OK"}, "set3");
   }

   {  // get
      resp::response_blob_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result,  std::string {}, "get3");
   }

   {  // quit
      resp::response_simple_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, std::string {"OK"}, "quit");
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
   std::string test_bulk(10000, 'a');

   std::string bulk;
   bulk += "$";
   bulk += std::to_string(std::size(test_bulk));
   bulk += "\r\n";
   bulk += test_bulk;
   bulk += "\r\n";

   // Redis answer - Expected vector.
   std::vector<std::pair<std::string, std::vector<std::string>>> payloads
   { {{"+OK\r\n"},                                           {"OK"}}
   , {{":3\r\n"},                                            {"3"}}
   , {{"*3\r\n$3\r\none\r\n$3\r\ntwo\r\n$5\r\nthree\r\n"},   {"one", "two", "three"}}
   , {{"*0\r\n"},                                            {}}
   , {{"$2\r\nhh\r\n"},                                      {"hh"}}
   , {{"$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n"},         {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}}
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
   , {{"$?\r\n;0\r\n"}, {}}
   //, {{bulk}, {test_bulk}}
   };

   std::string buffer;
   for (auto const& e : payloads) {
      test_tcp_socket ts {e.first};
      resp::response_vector<std::string> res;
      co_await resp::async_read(ts, buffer, res);
      if (e.second != res.result) {
        std::cout
	   << "Error: " << std::size(e.second)
	   << " " << std::size(res.result) << std::endl;
      } else {
        std::cout << "Success: Offline tests." << std::endl;
      }
   }
}

int main(int argc, char* argv[])
{
   net::io_context ioc {1};
   co_spawn(ioc, offline(), net::detached);
   co_spawn(ioc, test_list(), net::detached);
   co_spawn(ioc, test_set(), net::detached);
   ioc.run();
}

