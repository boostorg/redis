/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <map>

#include <aedis/aedis.hpp>

#include "test_stream.hpp"
#include "check.hpp"

// TODO: Use Beast test_stream and instantiate the test socket only
// once.

namespace net = aedis::net;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;
using test_tcp_socket = net::use_awaitable_t<>::as_default_on_t<aedis::test_stream<aedis::net::system_executor>>;

namespace this_coro = net::this_coro;

using namespace aedis;
using namespace aedis::resp3;

std::vector<node> gresp;

//-------------------------------------------------------------------

void simple_string_sync()
{
   test_tcp_socket ts {"+OK\r\n"};
   std::string rbuffer;
   auto dbuffer = net::dynamic_buffer(rbuffer);

   {  
      node result;
      resp3::read(ts, dbuffer, adapt(result));
      node expected {resp3::type::simple_string, 1UL, 0UL, {"OK"}};
      check_equal(result, expected, "simple_string (node-sync)");
   }

   {  
      std::string result;
      resp3::read(ts, dbuffer, adapt(result));
      std::string expected {"OK"};
      check_equal(result, expected, "simple_string (string-sync)");
   }

   {  
      std::optional<std::string> result;
      resp3::read(ts, dbuffer, adapt(result));
      std::optional<std::string> expected {"OK"};
      check_equal(result, expected, "simple_string (optional-string-sync)");
   }
}

void simple_string_sync_empty()
{
   test_tcp_socket ts {"+\r\n"};
   std::string rbuffer;
   auto dbuffer = net::dynamic_buffer(rbuffer);

   {  
      node result;
      resp3::read(ts, dbuffer, adapt(result));
      node expected {resp3::type::simple_string, 1UL, 0UL, {""}};
      check_equal(result, expected, "simple_string (empty-node-sync)");
   }

   {  
      std::string result;
      resp3::read(ts, dbuffer, adapt(result));
      std::string expected {""};
      check_equal(result, expected, "simple_string (empty-string-sync)");
   }

   {  
      std::optional<std::string> result;
      resp3::read(ts, dbuffer, adapt(result));
      std::optional<std::string> expected {""};
      check_equal(result, expected, "simple_string (empty-optional-string-sync)");
   }
}

net::awaitable<void> simple_string_async()
{
   {  // Small string (node-async)
      std::string buf;
      test_tcp_socket ts {"+OK\r\n"};
      node result;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(result));
      node expected {resp3::type::simple_string, 1UL, 0UL, {"OK"}};
      check_equal(result, expected, "simple_string (async)");
   }

   {  // Empty string
      std::string buf;
      test_tcp_socket ts {"+\r\n"};
      gresp.clear();
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      std::vector<node> expected
	 { {resp3::type::simple_string, 1UL, 0UL, {}} };
      check_equal(gresp, expected, "simple_string (empty)");
   }

   //{  // Large String (Failing because of my test stream)
   //   std::string buffer;
   //   std::string str(10000, 'a');
   //   std::string cmd;
   //   cmd += '+';
   //   cmd += str;
   //   cmd += "\r\n";
   //   test_tcp_socket ts {cmd};
   //   resp3::detail::simple_string_adapter res;
   //   co_await resp3::async_read(ts, buffer, res);
   //   check_equal(res.result, str, "simple_string (large)");
   //   //check_equal(res.attribute.value, {}, "simple_string (empty attribute)");
   //}
}

net::awaitable<void> test_number()
{
   using namespace aedis;
   std::string buf;
   {  // int
      std::string cmd {":-3\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
        { {resp3::type::number, 1UL, 0UL, {"-3"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "number (int)");
   }

   {  // unsigned
      std::string cmd {":3\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
        { {resp3::type::number, 1UL, 0UL, {"3"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "number (unsigned)");
   }

   {  // std::size_t
      std::string cmd {":1111111\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
        { {resp3::type::number, 1UL, 0UL, {"1111111"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "number (std::size_t)");
   }
}

net::awaitable<void> test_array()
{
   using namespace aedis;
   std::string buf;
   {  // String
      std::string cmd {"*3\r\n$3\r\none\r\n$3\r\ntwo\r\n$5\r\nthree\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::array,       3UL, 0UL, {}}
	 , {resp3::type::blob_string, 1UL, 1UL, {"one"}}
	 , {resp3::type::blob_string, 1UL, 1UL, {"two"}}
	 , {resp3::type::blob_string, 1UL, 1UL, {"three"}}
         };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "array");
   }

   //{  // int
   //   std::string cmd {"*3\r\n$1\r\n1\r\n$1\r\n2\r\n$1\r\n3\r\n"};
   //   test_tcp_socket ts {cmd};
   //   resp3::flat_array_int_type buffer;
   //   resp3::flat_array_int_adapter res{&buffer};
   //   co_await async_read(ts, buf, res);
   //   check_equal(buffer, {1, 2, 3}, "array (int)");
   //}

   {
      std::string cmd {"*0\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::array, 0UL, 0UL, {}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "array (empty)");
   }
}

net::awaitable<void> test_blob_string()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"$2\r\nhh\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::blob_string, 1UL, 0UL, {"hh"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "blob_string");
   }

   {
      std::string cmd {"$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::blob_string, 1UL, 0UL, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "blob_string (with separator)");
   }

   {
      std::string cmd {"$0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::blob_string, 1UL, 0UL, {}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "blob_string (size 0)");
   }
}

net::awaitable<void> test_simple_error()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"-Error\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::simple_error, 1UL, 0UL, {"Error"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "simple_error (message)");
   }
}

net::awaitable<void> test_floating_point()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {",1.23\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node> resp;
      std::vector<node> expected
	 { {resp3::type::doublean, 1UL, 0UL, {"1.23"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(resp));
      check_equal(resp, expected, "double");
   }

   {
      std::string cmd {",inf\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node> resp;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(resp));
      std::vector<node> expected
	 { {resp3::type::doublean, 1UL, 0UL, {"inf"}} };
      check_equal(resp, expected, "double (inf)");
   }

   {
      std::string cmd {",-inf\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node> resp;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(resp));
      std::vector<node> expected
	 { {resp3::type::doublean, 1UL, 0UL, {"-inf"}} };
      check_equal(resp, expected, "double (-inf)");
   }

}

net::awaitable<void> test_boolean()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"#f\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node> resp;
      std::vector<node> expected
	 { {resp3::type::boolean, 1UL, 0UL, {"f"}} };

      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(resp));
      check_equal(resp, expected, "bool (false)");
   }

   {
      std::string cmd {"#t\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node> resp;
      std::vector<node> expected
	 { {resp3::type::boolean, 1UL, 0UL, {"t"}} };

      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(resp));
      check_equal(resp, expected, "bool (true)");
   }
}

net::awaitable<void> test_blob_error()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"!21\r\nSYNTAX invalid syntax\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::blob_error, 1UL, 0UL, {"SYNTAX invalid syntax"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "blob_error (message)");
   }

   {
      std::string cmd {"!0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::blob_error, 1UL, 0UL, {}} };

      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "blob_error (empty message)");
   }
}

net::awaitable<void> test_verbatim_string()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"=15\r\ntxt:Some string\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::verbatim_string, 1UL, 0UL, {"txt:Some string"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "verbatim_string");
   }

   {
      std::string cmd {"=0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      std::vector<node> expected
	 { {resp3::type::verbatim_string, 1UL, 0UL, {}} };
      check_equal(gresp, expected, "verbatim_string (empty)");
   }
}

net::awaitable<void> test_set2()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"~5\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();

      std::vector<node> expected
      { {resp3::type::set,            5UL, 0UL, {}}
      , {resp3::type::simple_string,  1UL, 1UL, {"orange"}}
      , {resp3::type::simple_string,  1UL, 1UL, {"apple"}}
      , {resp3::type::simple_string,  1UL, 1UL, {"one"}}
      , {resp3::type::simple_string,  1UL, 1UL, {"two"}}
      , {resp3::type::simple_string,  1UL, 1UL, {"three"}}
      };

      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "test set (1)");
   }

   {
      std::string cmd {"~0\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();

      std::vector<node> expected
      { {resp3::type::set,  0UL, 0UL, {}}
      };

      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "test set (2)");
   }
}

net::awaitable<void> test_map()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"%7\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n6.0.9\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:203\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));

      std::vector<node> expected
      { {resp3::type::map,         7UL, 0UL, {}}
      , {resp3::type::blob_string, 1UL, 1UL, {"server"}}
      , {resp3::type::blob_string, 1UL, 1UL, {"redis"}}
      , {resp3::type::blob_string, 1UL, 1UL, {"version"}}
      , {resp3::type::blob_string, 1UL, 1UL, {"6.0.9"}}
      , {resp3::type::blob_string, 1UL, 1UL, {"proto"}}
      , {resp3::type::number,      1UL, 1UL, {"3"}}
      , {resp3::type::blob_string, 1UL, 1UL, {"id"}}
      , {resp3::type::number,      1UL, 1UL, {"203"}}
      , {resp3::type::blob_string, 1UL, 1UL, {"mode"}}
      , {resp3::type::blob_string, 1UL, 1UL, {"standalone"}}
      , {resp3::type::blob_string, 1UL, 1UL, {"role"}}
      , {resp3::type::blob_string, 1UL, 1UL, {"master"}}
      , {resp3::type::blob_string, 1UL, 1UL, {"modules"}}
      , {resp3::type::array,       0UL, 1UL, {}}
      };
      check_equal(gresp, expected, "test map");
   }

   {
      std::string cmd {"%0\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      std::vector<node> expected
      { {resp3::type::map, 0UL, 0UL, {}} };
      check_equal(gresp, expected, "test map (empty)");
   }
}

net::awaitable<void> test_streamed_string()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"$?\r\n;4\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node> expected
	 { {resp3::type::streamed_string_part, 1UL, 0UL, {"Hello world"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "streamed string");
   }

   {
      std::string cmd {"$?\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node> resp;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(resp));

      std::vector<node> expected
	 { {resp3::type::streamed_string_part, 1UL, 0UL, {}} };
      check_equal(resp, expected, "streamed string (empty)");
   }
}

//net::awaitable<void> offline()
//{
//   std::string buf;
//   //{
//   //   std::string cmd {"|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n"};
//   //   test_tcp_socket ts {cmd};
//   //   resp3::flat_radapter res;
//   //   co_await async_read(ts, buf, res);
//   //   check_equal(res.result, {"key-popularity", "a", "0.1923", "b", "0.0012"}, "attribute");
//   //}
//
//   //{
//   //   std::string cmd {">4\r\n+pubsub\r\n+message\r\n+foo\r\n+bar\r\n"};
//   //   test_tcp_socket ts {cmd};
//   //   resp3::flat_radapter res;
//   //   co_await async_read(ts, buf, res);
//   //   check_equal(res.result, {"pubsub", "message", "foo", "bar"}, "push type");
//   //}
//
//   //{
//   //   std::string cmd {">0\r\n"};
//   //   test_tcp_socket ts {cmd};
//   //   resp3::flat_radapter res;
//   //   co_await async_read(ts, buf, res);
//   //   check_equal(res.result, {}, "push type (empty)");
//   //}
//}

int main()
{
   simple_string_sync();
   simple_string_sync_empty();

   net::io_context ioc {1};

   co_spawn(ioc, simple_string_async(), net::detached);
   co_spawn(ioc, test_number(), net::detached);
   co_spawn(ioc, test_array(), net::detached);
   co_spawn(ioc, test_blob_string(), net::detached);
   co_spawn(ioc, test_simple_error(), net::detached);
   co_spawn(ioc, test_floating_point(), net::detached);
   co_spawn(ioc, test_boolean(), net::detached);
   co_spawn(ioc, test_blob_error(), net::detached);
   co_spawn(ioc, test_verbatim_string(), net::detached);
   co_spawn(ioc, test_set2(), net::detached);
   co_spawn(ioc, test_map(), net::detached);

   ioc.run();
}

