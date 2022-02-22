/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <map>

#include <aedis/src.hpp>
#include <aedis/aedis.hpp>
#include "test_stream.hpp"
#include "check.hpp"

// Consider using Beast test_stream and instantiate the test socket
// only once.

namespace net = aedis::net;
using tcp = net::ip::tcp;
using test_tcp_socket = net::use_awaitable_t<>::as_default_on_t<aedis::test_stream<aedis::net::system_executor>>;

namespace this_coro = net::this_coro;

using namespace aedis;
using namespace aedis::resp3;

using node_type = node<std::string>;
std::vector<node_type> gresp;

//-------------------------------------------------------------------

void simple_string_sync()
{
   test_tcp_socket ts {"+OK\r\n"};
   std::string rbuffer;
   auto dbuffer = net::dynamic_buffer(rbuffer);

   {  
      node_type result;
      resp3::read(ts, dbuffer, adapt(result));
      node_type expected {resp3::type::simple_string, 1UL, 0UL, {"OK"}};
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
      node_type result;
      resp3::read(ts, dbuffer, adapt(result));
      node_type expected {resp3::type::simple_string, 1UL, 0UL, {""}};
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
   test_tcp_socket ts {"+OK\r\n"};
   std::string rbuffer;
   auto dbuffer = net::dynamic_buffer(rbuffer);

   {
      node_type result;
      co_await resp3::async_read(ts, dbuffer, adapt(result));
      node_type expected {resp3::type::simple_string, 1UL, 0UL, {"OK"}};
      check_equal(result, expected, "simple_string (node-async)");
   }

   {
      std::string result;
      co_await resp3::async_read(ts, dbuffer, adapt(result));
      std::string expected {"OK"};
      check_equal(result, expected, "simple_string (string-async)");
   }

   {
      std::optional<std::string> result;
      co_await resp3::async_read(ts, dbuffer, adapt(result));
      std::optional<std::string> expected {"OK"};
      check_equal(result, expected, "simple_string (optional-string-async)");
   }
}

net::awaitable<void> simple_string_async_empty()
{
   test_tcp_socket ts {"+\r\n"};
   std::string rbuffer;
   auto dbuffer = net::dynamic_buffer(rbuffer);

   {
      node_type result;
      co_await resp3::async_read(ts, dbuffer, adapt(result));
      node_type expected {resp3::type::simple_string, 1UL, 0UL, {""}};
      check_equal(result, expected, "simple_string (empty-node-async)");
   }

   {
      std::string result;
      co_await resp3::async_read(ts, dbuffer, adapt(result));
      std::string expected {""};
      check_equal(result, expected, "simple_string (empty-string-async)");
   }

   {
      std::optional<std::string> result;
      co_await resp3::async_read(ts, dbuffer, adapt(result));
      std::optional<std::string> expected {""};
      check_equal(result, expected, "simple_string (empty-optional-string-async)");
   }
}

// TODO: Test a large simple string. For example
//   std::string str(10000, 'a');
//   std::string cmd;
//   cmd += '+';
//   cmd += str;
//   cmd += "\r\n";

//-------------------------------------------------------------------------

net::awaitable<void> test_simple_error_async()
{
   std::string buf;

   {
      test_tcp_socket ts {"-Error\r\n"};
      node_type result;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(result));
      node_type expected {resp3::type::simple_error, 1UL, 0UL, {"Error"}};
      check_equal(result, expected, "simple_error (node-async)");
   }

   {
      test_tcp_socket ts {"-Error\r\n"};
      std::string result;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(result));
      std::string expected {"Error"};
      check_equal(result, expected, "simple_error (string-async)");
   }

   {
      test_tcp_socket ts {"-\r\n"};
      std::string result;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(result));
      std::string expected {""};
      check_equal(result, expected, "simple_error (empty-string-async)");
   }

   // TODO: Test with optional.
   // TODO: Test with a very long string?
}

//-------------------------------------------------------------------------

net::awaitable<void> test_number()
{
   std::string buf;

   {
      test_tcp_socket ts {":-3\r\n"};
      node_type result;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(result));
      node_type expected {resp3::type::number, 1UL, 0UL, {"-3"}};
      check_equal(result, expected, "number (node-async)");
   }

   {
      test_tcp_socket ts {":-3\r\n"};
      long long result;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(result));
      check_equal(result, -3LL, "number (signed-async)");
   }

   {
      test_tcp_socket ts {":3\r\n"};
      std::size_t result;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(result));
      check_equal(result, 3UL, "number (unsigned-async)");
   }
}

//-------------------------------------------------------------------------

net::awaitable<void> array_async()
{
   test_tcp_socket ts {"*3\r\n$2\r\n11\r\n$2\r\n22\r\n$1\r\n3\r\n"};
   std::string buf;
   auto dbuf = net::dynamic_buffer(buf);

   {
      std::vector<node_type> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));

      std::vector<node_type> expected
	 { {resp3::type::array,       3UL, 0UL, {}}
	 , {resp3::type::blob_string, 1UL, 1UL, {"11"}}
	 , {resp3::type::blob_string, 1UL, 1UL, {"22"}}
	 , {resp3::type::blob_string, 1UL, 1UL, {"3"}}
         };

      check_equal(result, expected, "array (node-async)");
   }

   {
      std::vector<int> result;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(result));

      std::vector<int> expected {11, 22, 3};
      check_equal(result, expected, "array (int-async)");
   }

   {
      test_tcp_socket ts {"*0\r\n"};

      std::vector<int> result;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(result));

      std::vector<int> expected;
      check_equal(result, expected, "array (empty)");
   }
}

//-------------------------------------------------------------------------

net::awaitable<void> test_blob_string()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {"$2\r\nhh\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node_type> expected
	 { {resp3::type::blob_string, 1UL, 0UL, {"hh"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "blob_string");
   }

   {
      std::string cmd {"$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node_type> expected
	 { {resp3::type::blob_string, 1UL, 0UL, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "blob_string (with separator)");
   }

   {
      std::string cmd {"$0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node_type> expected
	 { {resp3::type::blob_string, 1UL, 0UL, {}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "blob_string (size 0)");
   }
}

net::awaitable<void> test_floating_point()
{
   using namespace aedis;
   std::string buf;
   {
      std::string cmd {",1.23\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node_type> resp;
      std::vector<node_type> expected
	 { {resp3::type::doublean, 1UL, 0UL, {"1.23"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(resp));
      check_equal(resp, expected, "double");
   }

   {
      std::string cmd {",inf\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node_type> resp;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(resp));
      std::vector<node_type> expected
	 { {resp3::type::doublean, 1UL, 0UL, {"inf"}} };
      check_equal(resp, expected, "double (inf)");
   }

   {
      std::string cmd {",-inf\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node_type> resp;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(resp));
      std::vector<node_type> expected
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
      std::vector<node_type> resp;
      std::vector<node_type> expected
	 { {resp3::type::boolean, 1UL, 0UL, {"f"}} };

      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(resp));
      check_equal(resp, expected, "bool (false)");
   }

   {
      std::string cmd {"#t\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node_type> resp;
      std::vector<node_type> expected
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
      std::vector<node_type> expected
	 { {resp3::type::blob_error, 1UL, 0UL, {"SYNTAX invalid syntax"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "blob_error (message)");
   }

   {
      std::string cmd {"!0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      std::vector<node_type> expected
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
      std::vector<node_type> expected
	 { {resp3::type::verbatim_string, 1UL, 0UL, {"txt:Some string"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "verbatim_string");
   }

   {
      std::string cmd {"=0\r\n\r\n"};
      test_tcp_socket ts {cmd};
      gresp.clear();
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      std::vector<node_type> expected
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

      std::vector<node_type> expected
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

      std::vector<node_type> expected
      { {resp3::type::set,  0UL, 0UL, {}}
      };

      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "test set (2)");
   }
}

net::awaitable<void> test_flat_map_async()
{
   std::string buf;
   auto dbuf = net::dynamic_buffer(buf);

   {
      test_tcp_socket ts {"%3\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n6.0.9\r\n$5\r\nproto\r\n:3\r\n"};
      std::map<std::string, std::string> result;
      co_await resp3::async_read(ts, dbuf, adapt(result));

      std::map<std::string, std::string> expected
      { {"server", "redis"}
      , {"version", "6.0.9"}
      , {"proto", "3"}
      };

      check_equal(result, expected, "map (flat-map-async)");
   }

   {
      test_tcp_socket ts {"%0\r\n"};
      std::map<std::string, std::string> result;
      co_await resp3::async_read(ts, dbuf, adapt(result));
      std::map<std::string, std::string> expected;
      check_equal(result, expected, "map (flat-empty-map-async)");
   }

   {
      // TODO: Why do we get a crash when %3. It should produce an
      // error instead.
      test_tcp_socket ts {"%2\r\n$6\r\nserver\r\n$2\r\n10\r\n$7\r\nversion\r\n$2\r\n30\r\n"};
      std::map<std::string, int> result;
      co_await resp3::async_read(ts, dbuf, adapt(result));

      std::map<std::string, int> expected
      { {"server", 10}
      , {"version", 30}
      };

      check_equal(result, expected, "map (flat-map-string-int-async)");
   }

   // TODO: Test optional map.
   // TODO: Test serializaition with different key and value types.
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

      std::vector<node_type> expected
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
      std::vector<node_type> expected
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
      std::vector<node_type> expected
	 { {resp3::type::streamed_string_part, 1UL, 0UL, {"Hello world"}} };
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(gresp));
      check_equal(gresp, expected, "streamed string");
   }

   {
      std::string cmd {"$?\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node_type> resp;
      co_await resp3::async_read(ts, net::dynamic_buffer(buf), adapt(resp));

      std::vector<node_type> expected
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

net::awaitable<void> optional_async()
{
   test_tcp_socket ts {"_\r\n"};
   std::string buf;
   auto dbuf = net::dynamic_buffer(buf);

   {
      node_type result;
      co_await resp3::async_read(ts, dbuf, adapt(result));
      node_type expected {resp3::type::null, 1UL, 0UL, {""}};
      check_equal(result, expected, "optional (node-async)");
   }

   {
      int result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));

      //auto const expected = make_error_code(adapter::error::null);
      //std::cout << expected.message() << std::endl;
      // TODO: Convert to std::error_code.
      //check_equal(ec, expected, "optional (int-async)");
   }

   {
      std::optional<int> result;
      co_await resp3::async_read(ts, dbuf, adapt(result));
      std::optional<int> expected;
      check_equal(result, expected, "optional (optional-int-async)");
   }

   {
      std::optional<std::string> result;
      co_await resp3::async_read(ts, dbuf, adapt(result));
      std::optional<std::string> expected;
      check_equal(result, expected, "optional (optional-int-async)");
   }
}

int main()
{
   simple_string_sync();
   simple_string_sync_empty();

   net::io_context ioc {1};

   co_spawn(ioc, simple_string_async(), net::detached);
   co_spawn(ioc, simple_string_async_empty(), net::detached);
   co_spawn(ioc, test_simple_error_async(), net::detached);
   co_spawn(ioc, test_number(), net::detached);
   co_spawn(ioc, test_map(), net::detached);
   co_spawn(ioc, test_flat_map_async(), net::detached);
   co_spawn(ioc, optional_async(), net::detached);

   co_spawn(ioc, array_async(), net::detached);
   co_spawn(ioc, test_blob_string(), net::detached);
   co_spawn(ioc, test_floating_point(), net::detached);
   co_spawn(ioc, test_boolean(), net::detached);
   co_spawn(ioc, test_blob_error(), net::detached);
   co_spawn(ioc, test_verbatim_string(), net::detached);
   co_spawn(ioc, test_set2(), net::detached);

   ioc.run();
}

