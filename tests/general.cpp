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
      resp::response_simple_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "flushall");
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
      resp::response_simple_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "ltrim");
   }

   {  // lpop. Why a blob string instead of a number?
      resp::response_blob_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, {"3"}, "lpop");
   }

   {  // quit
      resp::response_simple_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "quit");
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
      check_equal(res.result, {"OK"}, "set1");
   }

   {  // get
      resp::response_blob_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, test_bulk1, "get1");
   }

   {  // set
      resp::response_simple_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "set1");
   }

   {  // get
      resp::response_blob_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, test_bulk2, "get2");
   }

   {  // set
      resp::response_simple_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "set3");
   }

   {  // get
      resp::response_blob_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result,  std::string {}, "get3");
   }

   {  // quit
      resp::response_simple_string res;
      co_await resp::async_read(socket, buffer, res);
      check_equal(res.result, {"OK"}, "quit");
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
   // TODO: Use Beast test_stream and instantiate the test socket only
   // once. Pass commands in a pipeline.

   std::string test_bulk(10000, 'a');

   std::string bulk;
   bulk += "$";
   bulk += std::to_string(std::size(test_bulk));
   bulk += "\r\n";
   bulk += test_bulk;
   bulk += "\r\n";

   std::vector<std::string> commands
   { "+OK\r\n"
   , ":3\r\n"
   , "*3\r\n$3\r\none\r\n$3\r\ntwo\r\n$5\r\nthree\r\n"
   , "*0\r\n"
   , "$2\r\nhh\r\n"
   , "$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n"
   , "$0\r\n\r\n"
   , "-Error\r\n"
   , ",1.23\r\n"
   , ",inf\r\n"
   , ",-inf\r\n"
   , "#f\r\n"
   , "#t\r\n"
   , "!21\r\nSYNTAX invalid syntax\r\n"
   , "!0\r\n"
   , "=15\r\ntxt:Some string\r\n"
   , "(3492890328409238509324850943850943825024385\r\n"
   , "~5\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n"
   , "~0\r\n"
   , "%7\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n6.0.9\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:203\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n"
   , "%0\r\n"
   , "|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n"
   , ">4\r\n+pubsub\r\n+message\r\n+foo\r\n+bar\r\n"
   , ">0\r\n"
   , "$?\r\n;4\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"
   , "$?\r\n;0\r\n"
   //, bulk
   };

   std::string buffer;
   {
      test_tcp_socket ts {commands[0]};
      resp::response_simple_string res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"OK"}, "simple_string");
   }

   {
      test_tcp_socket ts {commands[1]};
      resp::response_number<int> res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, 3, "number");
   }

   {
      test_tcp_socket ts {commands[2]};
      resp::response_array<std::string> res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"one", "two", "three"}, "array");
   }

   {
      test_tcp_socket ts {commands[3]};
      resp::response_array<std::string> res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {}, "array (size 0)");
   }

   {
      test_tcp_socket ts {commands[4]};
      resp::response_blob_string res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"hh"}, "blob_string");
   }

   {
      test_tcp_socket ts {commands[5]};
      resp::response_blob_string res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}, "blob_string (with separator)");
   }

   {
      test_tcp_socket ts {commands[6]};
      resp::response_blob_string res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {}, "blob_string (size 0)");
   }

   {
      test_tcp_socket ts {commands[7]};
      resp::response_base res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.message(), {"Error"}, "simple_error (message)");
      check_equal(res.get_error(), resp::error::simple_error, "simple_error (enum)");
   }

   {
      test_tcp_socket ts {commands[8]};
      resp::response_double res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"1.23"}, "double");
   }

   {
      test_tcp_socket ts {commands[9]};
      resp::response_double res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"inf"}, "double (inf)");
   }

   {
      test_tcp_socket ts {commands[10]};
      resp::response_double res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"-inf"}, "double (-inf)");
   }

   {
      test_tcp_socket ts {commands[11]};
      resp::response_bool res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, false, "bool (false)");
   }

   {
      test_tcp_socket ts {commands[12]};
      resp::response_bool res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, true, "bool (true)");
   }

   {
      test_tcp_socket ts {commands[13]};
      resp::response_base res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.message(), {"SYNTAX invalid syntax"}, "blob_error (message)");
      check_equal(res.get_error(), resp::error::blob_error, "blob_error (enum)");
   }

   {
      test_tcp_socket ts {commands[14]};
      resp::response_base res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.message(), {}, "blob_error (empty message)");
      check_equal(res.get_error(), resp::error::blob_error, "blob_error (enum)");
   }

   {
      test_tcp_socket ts {commands[15]};
      resp::response_verbatim_string res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"txt:Some string"}, "verbatim_string");
   }

   {
      test_tcp_socket ts {commands[16]};
      resp::response_big_number res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"3492890328409238509324850943850943825024385"}, "big number");
   }

   {
      test_tcp_socket ts {commands[17]};
      resp::response_set<std::string> res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"orange", "apple", "one", "two", "three"}, "set");
   }

   {
      test_tcp_socket ts {commands[17]};
      resp::response_flat_set<std::string> res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"orange", "apple", "one", "two", "three"}, "set (flat)");
   }

   {
      test_tcp_socket ts {commands[18]};
      resp::response_set<std::string> res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {}, "set (empty)");
   }

   {
      test_tcp_socket ts {commands[19]};
      resp::response_flat_map<std::string> res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"server", "redis", "version", "6.0.9", "proto", "3", "id", "203", "mode", "standalone", "role", "master", "modules"}, "map (flat)");
   }

   {
      test_tcp_socket ts {commands[20]};
      resp::response_flat_map<std::string> res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {}, "map (flat - empty)");
   }

   {  // TODO: Find a better way to deal with attributes.
      test_tcp_socket ts {commands[21]};
      resp::response_array<std::string> res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"key-popularity", "a", "0.1923", "b", "0.0012"}, "attribute");
   }

   {  // TODO: Find a better way to deal with push events.
      test_tcp_socket ts {commands[22]};
      resp::response_array<std::string> res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"pubsub", "message", "foo", "bar"}, "push type");
   }

   {  // TODO: Find a better way to deal with push events.
      test_tcp_socket ts {commands[23]};
      resp::response_array<std::string> res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {}, "push type (empty)");
   }

   {
      test_tcp_socket ts {commands[24]};
      resp::response_streamed_string res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {"Hello word"}, "streamed string");
   }

   {
      test_tcp_socket ts {commands[25]};
      resp::response_array<std::string> res;
      co_await resp::async_read(ts, buffer, res);
      check_equal(res.result, {}, "streamed string (empty)");
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

