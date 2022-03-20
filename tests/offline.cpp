/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <map>
#include <iostream>
#include <optional>

#include <boost/system/errc.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "test_stream.hpp"
#include "check.hpp"

// Consider using Beast test_stream and instantiate the test socket
// only once.
// TODO: Check the read buffer is empty after each test.
// TODO: Check adapter errors.
// TODO: Check resp3 errors.
// TODO: Separate test for node_type and adapter.

namespace net = aedis::net;
namespace resp3 = aedis::resp3;
using test_tcp_socket = net::use_awaitable_t<>::as_default_on_t<aedis::test_stream<aedis::net::system_executor>>;
using aedis::adapter::adapt;
using node_type = aedis::adapter::node<std::string>;

//-------------------------------------------------------------------

template <class Result>
struct expect {
   std::string in;
   Result expected;
   std::string name;
   boost::system::error_condition ec;
};

template <class Result>
void test_sync(expect<Result> e)
{
   std::string rbuffer;
   test_tcp_socket ts {e.in};
   Result result;
   boost::system::error_code ec;
   resp3::read(ts, net::dynamic_buffer(rbuffer), adapt(result), ec);
   expect_error(ec, e.ec);
   if (e.ec)
      return;
   check_empty(rbuffer);
   expect_eq(result, e.expected, e.name);
}

template <class Result>
net::awaitable<void>
test_async(expect<Result> e)
{
   std::string rbuffer;
   test_tcp_socket ts {e.in};
   Result result;
   boost::system::error_code ec;
   co_await resp3::async_read(ts, net::dynamic_buffer(rbuffer), adapt(result), net::redirect_error(net::use_awaitable, ec));
   expect_error(ec, e.ec);
   if (e.ec)
      co_return;
   check_empty(rbuffer);
   expect_eq(result, e.expected, e.name);
}

// TODO: Test a large simple string. For example
//   std::string str(10000, 'a');
//   std::string cmd;
//   cmd += '+';
//   cmd += str;
//   cmd += "\r\n";

//-------------------------------------------------------------------------

net::awaitable<void> test_async_array()
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

      expect_eq(result, expected, "array (node-async)");
   }

   {
      std::vector<int> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<int> expected {11, 22, 3};
      expect_eq(result, expected, "array (int-async)");
   }

   {
      test_tcp_socket ts {"*0\r\n"};

      std::vector<int> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<int> expected;
      expect_eq(result, expected, "array (empty)");
   }
}

net::awaitable<void> test_async_set()
{
   std::string buf;
   auto dbuf = net::dynamic_buffer(buf);

   {
      std::string cmd {"~5\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node_type> result;

      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<node_type> expected
      { {resp3::type::set,            5UL, 0UL, {}}
      , {resp3::type::simple_string,  1UL, 1UL, {"orange"}}
      , {resp3::type::simple_string,  1UL, 1UL, {"apple"}}
      , {resp3::type::simple_string,  1UL, 1UL, {"one"}}
      , {resp3::type::simple_string,  1UL, 1UL, {"two"}}
      , {resp3::type::simple_string,  1UL, 1UL, {"three"}}
      };

      expect_eq(result, expected, "test set (1)");
   }

   {
      std::string cmd {"~0\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node_type> result;

      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<node_type> expected { {resp3::type::set,  0UL, 0UL, {}} };
      expect_eq(result, expected, "test set (2)");
   }
}

net::awaitable<void> test_async_map()
{
   std::string buf;
   auto dbuf = net::dynamic_buffer(buf);

   {
      std::string cmd {"%7\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n6.0.9\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:203\r\n$4\r\nmode\r\n$10\r\nstandalone\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$7\r\nmodules\r\n*0\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node_type> result;

      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

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

      expect_eq(result, expected, "map.node");
   }

   {
      std::string cmd {"%0\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node_type> result;

      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<node_type> expected { {resp3::type::map, 0UL, 0UL, {}} };
      expect_eq(result, expected, "map.async.node.empty");
   }

   {
      test_tcp_socket ts {"%3\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n6.0.9\r\n$5\r\nproto\r\n:3\r\n"};
      std::map<std::string, std::string> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::map<std::string, std::string> expected
      { {"server", "redis"}
      , {"version", "6.0.9"}
      , {"proto", "3"}
      };

      expect_eq(result, expected, "map.async.map.string.string");
   }

   {
      test_tcp_socket ts {"%2\r\n$4\r\nkey1\r\n$2\r\n10\r\n$4\r\nkey2\r\n$2\r\n30\r\n"};
      std::map<std::string, int> result;

      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::map<std::string, int> expected
      { {"key1", 10}
      , {"key2", 30}
      };

      expect_eq(result, expected, "map.async.map.string.int");
   }

   {
      // Wrong number of elements should produce an error.
      test_tcp_socket ts {"%4\r\n$4\r\nkey1\r\n$2\r\n10\r\n$4\r\nkey2\r\n$2\r\n30\r\n"};

      std::map<std::string, int> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));

      boost::system::error_code expected = aedis::adapter::error::nested_unsupported;
      expect_eq(ec, expected, "map.async.map.error (nested unsupported)");
   }

   // TODO: Test optional map.
}

net::awaitable<void> test_streamed_string()
{
   using namespace aedis;
   std::string rbuffer;
   auto dbuf = net::dynamic_buffer(rbuffer);

   {
      std::string cmd {"$?\r\n;4\r\nHell\r\n;5\r\no wor\r\n;1\r\nd\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node_type> result;

      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<node_type> expected { {resp3::type::streamed_string_part, 1UL, 0UL, {"Hello world"}} };
      expect_eq(result, expected, "streamed_string.async");
   }

   {
      std::string cmd {"$?\r\n;0\r\n"};
      test_tcp_socket ts {cmd};
      std::vector<node_type> result;

      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<node_type> expected { {resp3::type::streamed_string_part, 1UL, 0UL, {}} };
      expect_eq(result, expected, "streamed_string.async.empty");
   }
}

net::awaitable<void> test_async_attribute()
{
   {
      test_tcp_socket ts{"|1\r\n+key-popularity\r\n%2\r\n$1\r\na\r\n,0.1923\r\n$1\r\nb\r\n,0.0012\r\n"};
      std::string rbuffer;
      auto dbuf = net::dynamic_buffer(rbuffer);

      std::vector<node_type> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<node_type> expected
         { {resp3::type::attribute,     1UL, 0UL, {}}
         , {resp3::type::simple_string, 1UL, 1UL, "key-popularity"}
         , {resp3::type::map,           2UL, 1UL, {}}
         , {resp3::type::blob_string,   1UL, 2UL, "a"}
         , {resp3::type::doublean,      1UL, 2UL, "0.1923"}
         , {resp3::type::blob_string,   1UL, 2UL, "b"}
         , {resp3::type::doublean,      1UL, 2UL, "0.0012"}
         };

      expect_eq(result, expected, "attribute.async");
   }

   {
      test_tcp_socket ts{"|0\r\n"};
      std::string rbuffer;
      auto dbuf = net::dynamic_buffer(rbuffer);

      std::vector<node_type> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<node_type> expected{{resp3::type::attribute, 0UL, 0UL, {}}};
      expect_eq(result, expected, "attribute.async.empty");
   }
}

net::awaitable<void> test_async_push()
{
   {
      test_tcp_socket ts{">4\r\n+pubsub\r\n+message\r\n+some-channel\r\n+some message\r\n"};
      std::string rbuffer;
      auto dbuffer = net::dynamic_buffer(rbuffer);

      std::vector<node_type> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuffer, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<node_type> expected
         { {resp3::type::push,          4UL, 0UL, {}}
         , {resp3::type::simple_string, 1UL, 1UL, "pubsub"}
         , {resp3::type::simple_string, 1UL, 1UL, "message"}
         , {resp3::type::simple_string, 1UL, 1UL, "some-channel"}
         , {resp3::type::simple_string, 1UL, 1UL, "some message"}
         };

      expect_eq(result, expected, "push.async");
   }

   {
      test_tcp_socket ts{">0\r\n"};
      std::string rbuffer;
      auto dbuffer = net::dynamic_buffer(rbuffer);

      std::vector<node_type> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuffer, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);

      std::vector<node_type> expected { {resp3::type::push,          0UL, 0UL, {}} };
      expect_eq(result, expected, "push.async.empty");
   }
}

net::awaitable<void> test_async_optional()
{
   test_tcp_socket ts {"_\r\n"};
   std::string buf;
   auto dbuf = net::dynamic_buffer(buf);

   {
      int result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      boost::system::error_code expected = aedis::adapter::error::null;
      expect_eq(ec, expected, "optional.async.int.error (null)");
   }

   {
      std::optional<int> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);
      std::optional<int> expected;
      expect_eq(result, expected, "optional (optional-int-async)");
   }

   {
      std::optional<std::string> result;
      boost::system::error_code ec;
      co_await resp3::async_read(ts, dbuf, adapt(result), net::redirect_error(net::use_awaitable, ec));
      expect_error(ec);
      std::optional<std::string> expected;
      expect_eq(result, expected, "optional (optional-int-async)");
   }
}

void simple()
{
   std::vector<expect<node_type>> const simple
   // Simple string
   { {"+OK\r\n", node_type{resp3::type::simple_string, 1UL, 0UL, {"OK"}}, "simple_string.node"}
   , {"+\r\n", node_type{resp3::type::simple_string, 1UL, 0UL, {""}}, "simple_string.node.empty"}
   // Simple error
   , {"-Error\r\n", node_type{resp3::type::simple_error, 1UL, 0UL, {"Error"}}, "simple_error.node"}
   , {"-\r\n", node_type{resp3::type::simple_error, 1UL, 0UL, {""}}, "simple_error.node.empty"}
   // Number
   , {":-3\r\n", node_type{resp3::type::number, 1UL, 0UL, {"-3"}}, "number.node (negative)"}
   , {":3\r\n", node_type{resp3::type::number, 1UL, 0UL, {"3"}}, "number.node (positive)"}
   // Blob string. TODO: test with a long that require many calls to async_read_some (fix test_stream first).
   , {"$2\r\nhh\r\n", node_type{resp3::type::blob_string, 1UL, 0UL, {"hh"}}, "blob_string.node"}
   , {"$26\r\nhhaa\aaaa\raaaaa\r\naaaaaaaaaa\r\n", node_type{resp3::type::blob_string, 1UL, 0UL, {"hhaa\aaaa\raaaaa\r\naaaaaaaaaa"}}, "blob_string.node (with separator)"}
   , {"$0\r\n\r\n", node_type{resp3::type::blob_string, 1UL, 0UL, {}}, "blob_string.node.empty"}
   // Bool
   , {"#f\r\n", node_type{resp3::type::boolean, 1UL, 0UL, {"f"}}, "bool.node (false)"}
   , {"#t\r\n", node_type{resp3::type::boolean, 1UL, 0UL, {"t"}}, "bool.node (true)"}
   // Double
   , {",1.23\r\n", node_type{resp3::type::doublean, 1UL, 0UL, {"1.23"}}, "double.node"}
   , {",inf\r\n", node_type{resp3::type::doublean, 1UL, 0UL, {"inf"}}, "double.node (inf)"}
   , {",-inf\r\n", node_type{resp3::type::doublean, 1UL, 0UL, {"-inf"}}, "double.node (-inf)"}
   // Blob error
   , {"!21\r\nSYNTAX invalid syntax\r\n", node_type{resp3::type::blob_error, 1UL, 0UL, {"SYNTAX invalid syntax"}}, "blob_error"}
   , {"!0\r\n\r\n", node_type{resp3::type::blob_error, 1UL, 0UL, {}}, "blob_error.empty"}
   // Verbatim string
   , {"=15\r\ntxt:Some string\r\n", node_type{resp3::type::verbatim_string, 1UL, 0UL, {"txt:Some string"}}, "verbatim_string"}
   , {"=0\r\n\r\n", node_type{resp3::type::verbatim_string, 1UL, 0UL, {}}, "verbatim_string.empty"}
   // Null
   , {"_\r\n", node_type{resp3::type::null, 1UL, 0UL, {}}, "null"}
   // Big number
   , {"(3492890328409238509324850943850943825024385\r\n", node_type{resp3::type::big_number, 1UL, 0UL, {"3492890328409238509324850943850943825024385"}}, "big_number"}
   , {"(\r\n", node_type{resp3::type::big_number, 1UL, 0UL, {}}, "big_number.empty"}
   };

   // Tests
   for (auto const& e: simple)
      test_sync(e);

   net::io_context ioc {1};
   for (auto const& e: simple)
     co_spawn(ioc, test_async(e), net::detached);

   ioc.run();
}

void adapter_simple()
{
   std::optional<std::string> ok1, ok2;
   ok1 = "OK";
   ok2 = "";

   std::optional<int> ok3;
   ok3 = 11;

   net::io_context ioc {1};

   // Adapter for int (number).
   test_sync(expect<int>{":11\r\n", int{11}, "adapter.number.int"});
   test_sync(expect<int>{":adf\r\n", int{11}, "adapter.number.int", boost::system::errc::errc_t::invalid_argument});
   test_sync(expect<int>{"%11\r\n", int{}, "adapter.number.invalid", aedis::adapter::make_error_condition(aedis::adapter::error::expects_simple_type)});

   test_sync(expect<std::optional<int>>{":11\r\n", ok3, "adapter.number.optional.int"});
   test_sync(expect<std::optional<int>>{"_\r\n", std::optional<int>{}, "adapter.number.optional.int (null)"});
   // Test the above but expecting adapter errors.

   // Adapter for std::string (simple_string)
   test_sync(expect<std::string>{"+OK\r\n", std::string{"OK"}, "simple_string.sync.string"});
   test_sync(expect<std::string>{"+\r\n", std::string{""}, "simple_string.sync.string.empty"});
   test_sync(expect<std::optional<std::string>>{"+OK\r\n", std::optional<std::string>{"OK"}, "simple_string.sync.optional"});
   test_sync(expect<std::optional<std::string>>{"+\r\n", std::optional<std::string>{""}, "simple_string.sync.optional.empty"});

   co_spawn(ioc, test_async(expect<std::string>{"+OK\r\n", std::string{"OK"}, "simple_string.async.string"}), net::detached);
   co_spawn(ioc, test_async(expect<std::string>{"+\r\n", std::string{""}, "simple_string.async.string.empty"}), net::detached);
   co_spawn(ioc, test_async(expect<std::optional<std::string>>{"+OK\r\n", ok1, "simple_string.async.string.optional"}), net::detached);
   co_spawn(ioc, test_async(expect<std::optional<std::string>>{"+\r\n", ok2, "simple_string.async.string.optional.empty"}), net::detached);

   // RESP3 tests.
   test_sync(expect<int>{"#11\r\n", int{}, "adapter.bool.invalid", aedis::resp3::make_error_condition(aedis::resp3::error::unexpected_bool_value)});
   test_sync(expect<int>{"s11\r\n", int{}, "adapter.number.invalid", aedis::resp3::make_error_condition(aedis::resp3::error::invalid_type)});

   ioc.run();
}

void aggregate()
{
   net::io_context ioc {1};

   co_spawn(ioc, test_async_map(), net::detached);
   co_spawn(ioc, test_async_optional(), net::detached);
   co_spawn(ioc, test_async_attribute(), net::detached);
   co_spawn(ioc, test_async_push(), net::detached);
   co_spawn(ioc, test_async_array(), net::detached);
   co_spawn(ioc, test_async_set(), net::detached);
   ioc.run();
}

int main()
{
   simple();
   adapter_simple();
   aggregate();
}

