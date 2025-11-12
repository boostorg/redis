//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/sentinel_resolve_fsm.hpp>
#include <boost/redis/impl/parse_sentinel_response.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/error_code.hpp>

#include "sansio_utils.hpp"

#include <iterator>

using namespace boost::redis;
namespace asio = boost::asio;
using detail::sentinel_resolve_fsm;
using detail::sentinel_action_type;
using detail::sentinel_action;
using detail::connection_state;
using boost::system::error_code;
using boost::asio::cancellation_type_t;

static char const* to_string(sentinel_action_type t)
{
   switch (t) {
      case sentinel_action_type::done:    return "sentinel_action_type::done";
      case sentinel_action_type::connect: return "sentinel_action_type::connect";
      case sentinel_action_type::request: return "sentinel_action_type::request";
      default:                            return "sentinel_action_type::<invalid type>";
   }
}

// Operators
namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, sentinel_action_type type)
{
   os << to_string(type);
   return os;
}

bool operator==(sentinel_action lhs, sentinel_action rhs) noexcept
{
   if (lhs.type() != rhs.type())
      return false;
   else if (lhs.type() == sentinel_action_type::done)
      return lhs.error() == rhs.error();
   else if (lhs.type() == sentinel_action_type::connect)
      return lhs.connect_addr() == rhs.connect_addr();
   else
      return true;
}

std::ostream& operator<<(std::ostream& os, sentinel_action act)
{
   os << "exec_action{ .type=" << act.type();
   if (act.type() == sentinel_action_type::done)
      os << ", .error=" << act.error();
   else if (act.type() == sentinel_action_type::connect)
      os << ", .addr=" << act.connect_addr().host << ":" << act.connect_addr().port;
   return os << " }";
}

}  // namespace boost::redis::detail

namespace boost::redis {

std::ostream& operator<<(std::ostream& os, const address& addr)
{
   return os << "address{ .host=" << addr.host << ", .port=" << addr.port << " }";
}

}  // namespace boost::redis

namespace {

// TODO: duplicated
std::vector<resp3::node> from_resp3(const std::vector<std::string_view>& responses)
{
   std::vector<resp3::node> nodes;
   auto adapter = detail::make_vector_adapter(nodes);

   for (std::string_view resp : responses) {
      resp3::parser p;
      error_code ec;
      bool ok = resp3::parse(p, resp, adapter, ec);
      BOOST_TEST(ok);
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST(p.done());
   }

   return nodes;
}

struct fixture : detail::log_fixture {
   connection_state st{{make_logger()}};
   sentinel_resolve_fsm fsm;

   fixture()
   {
      st.cfg.sentinel.addresses = {
         {"host1", "1000"},
         {"host2", "2000"},
         {"host3", "3000"},
      };
      st.cfg.sentinel.master_name = "mymaster";

      st.sentinels = st.cfg.sentinel.addresses;  // performed by run
   }
};

void test_success()
{
   // Setup
   fixture fix;

   // Initiate. We should connect to the 1st sentinel
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, (address{"host1", "1000"}));

   // Now send the request
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, sentinel_action::request());
   fix.st.sentinel_resp_nodes = from_resp3({
      // clang-format off
      "*2\r\n$9\r\ntest.host\r\n$4\r\n6380\r\n",
      "*1\r\n"
         "%2\r\n"
            "$2\r\nip\r\n$8\r\nhost.one\r\n$4\r\nport\r\n$5\r\n26380\r\n",
      // clang-format on
   });

   // We received a valid request, so we're done
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code());

   // The master's address is stored
   BOOST_TEST_EQ(fix.st.cfg.addr, (address{"test.host", "6380"}));

   // The Sentinel list is updated
   const address expected_sentinels[] = {
      {"host1",    "1000" },
      {"host.one", "26380"},
      {"host2",    "2000" },
      {"host3",    "3000" },
   };
   BOOST_TEST_ALL_EQ(
      fix.st.sentinels.begin(),
      fix.st.sentinels.end(),
      std::begin(expected_sentinels),
      std::end(expected_sentinels));

   // Logs
   fix.check_log({
      {logger::level::info,  "Trying to resolve the address of master 'mymaster' using Sentinel"   },
      {logger::level::debug, "Trying to contact Sentinel at host1:1000"                            },
      {logger::level::debug, "Executing Sentinel request at host1:1000"                            },
      {logger::level::info,  "Sentinel at host1:1000 resolved the server address to test.host:6380"},
   });
}

void test_success_replica()
{
   // Setup
   fixture fix;
   fix.st.cfg.sentinel.server_role = role::replica;
   fix.st.eng.seed(183984887232u);  // this returns index 1. TODO: is this portable?

   // Initiate. We should connect to the 1st sentinel
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, (address{"host1", "1000"}));

   // Now send the request
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, sentinel_action::request());
   fix.st.sentinel_resp_nodes = from_resp3({
      // clang-format off
      "*2\r\n$9\r\ntest.host\r\n$4\r\n6380\r\n",
      "*3\r\n"
         "%2\r\n"
            "$2\r\nip\r\n$11\r\nreplica.one\r\n$4\r\nport\r\n$4\r\n6379\r\n"
         "%2\r\n"
            "$2\r\nip\r\n$11\r\nreplica.two\r\n$4\r\nport\r\n$4\r\n6379\r\n"
         "%2\r\n"
            "$2\r\nip\r\n$11\r\nreplica.thr\r\n$4\r\nport\r\n$4\r\n6379\r\n",
      "*0\r\n"
      // clang-format on
   });

   // We received a valid request, so we're done
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code());

   // The address of one of the replicas is stored
   BOOST_TEST_EQ(fix.st.cfg.addr, (address{"replica.two", "6379"}));

   // Logs
   fix.check_log({
      // clang-format off
      {logger::level::info,  "Trying to resolve the address of a replica of master 'mymaster' using Sentinel" },
      {logger::level::debug, "Trying to contact Sentinel at host1:1000"                                       },
      {logger::level::debug, "Executing Sentinel request at host1:1000"                                       },
      {logger::level::info,  "Sentinel at host1:1000 resolved the server address to replica.two:6379"         },
      // clang-format on
   });
}

// The first Sentinel fails connection, but subsequent ones succeed
void test_one_connect_error()
{
   // Setup
   fixture fix;

   // Initiate. We should connect to the 1st sentinel
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, (address{"host1", "1000"}));

   // This errors, so we connect to the 2nd sentinel
   act = fix.fsm.resume(fix.st, error::connect_timeout, cancellation_type_t::none);
   BOOST_TEST_EQ(act, (address{"host2", "2000"}));

   // Now send the request
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, sentinel_action::request());
   fix.st.sentinel_resp_nodes = from_resp3({
      "*2\r\n$9\r\ntest.host\r\n$4\r\n6380\r\n",
      "*0\r\n",
   });

   // We received a valid request, so we're done
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code());

   // The master's address is stored
   BOOST_TEST_EQ(fix.st.cfg.addr, (address{"test.host", "6380"}));

   // Logs
   fix.check_log({
      // clang-format off
      {logger::level::info,  "Trying to resolve the address of master 'mymaster' using Sentinel"   },
      {logger::level::debug, "Trying to contact Sentinel at host1:1000"                            },
      {logger::level::info,  "Failed to connect to Sentinel at host1:1000: Connect timeout. [boost.redis:18]" },
      {logger::level::debug, "Trying to contact Sentinel at host2:2000"                            },
      {logger::level::debug, "Executing Sentinel request at host2:2000"                            },
      {logger::level::info,  "Sentinel at host2:2000 resolved the server address to test.host:6380"},
      // clang-format on
   });
}

// The first Sentinel fails while executing the request, but subsequent ones succeed
void test_one_request_network_error()
{
   // Setup
   fixture fix;

   // Initiate, connect to the 1st Sentinel, and send the request
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, (address{"host1", "1000"}));
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, sentinel_action::request());

   // It fails, so we connect to the 2nd sentinel. This one succeeds
   act = fix.fsm.resume(fix.st, error::write_timeout, cancellation_type_t::none);
   BOOST_TEST_EQ(act, (address{"host2", "2000"}));
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, sentinel_action::request());
   fix.st.sentinel_resp_nodes = from_resp3({
      "*2\r\n$9\r\ntest.host\r\n$4\r\n6380\r\n",
      "*0\r\n",
   });
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code());

   // The master's address is stored
   BOOST_TEST_EQ(fix.st.cfg.addr, (address{"test.host", "6380"}));

   // Logs
   fix.check_log({
      // clang-format off
      {logger::level::info,  "Trying to resolve the address of master 'mymaster' using Sentinel"   },
      {logger::level::debug, "Trying to contact Sentinel at host1:1000"                            },
      {logger::level::debug, "Executing Sentinel request at host1:1000"                            },
      {logger::level::info,  "Failed to execute request at Sentinel host1:1000: Timeout while writing data to the server. [boost.redis:27]"},
      {logger::level::debug, "Trying to contact Sentinel at host2:2000"                            },
      {logger::level::debug, "Executing Sentinel request at host2:2000"                            },
      {logger::level::info,  "Sentinel at host2:2000 resolved the server address to test.host:6380"},
      // clang-format on
   });
}

// The first Sentinel doesn't know about the master, but others do
void test_one_master_unknown()
{
   // Setup
   fixture fix;

   // Initiate, connect to the 1st Sentinel, and send the request
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, (address{"host1", "1000"}));
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, sentinel_action::request());
   fix.st.sentinel_resp_nodes = from_resp3({
      "_\r\n",
      "-ERR unknown master\r\n",
   });

   // It doesn't know about our master, so we connect to the 2nd sentinel.
   // This one succeeds
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, (address{"host2", "2000"}));
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, sentinel_action::request());
   fix.st.sentinel_resp_nodes = from_resp3({
      "*2\r\n$9\r\ntest.host\r\n$4\r\n6380\r\n",
      "*0\r\n",
   });
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code());

   // The master's address is stored
   BOOST_TEST_EQ(fix.st.cfg.addr, (address{"test.host", "6380"}));

   // Logs
   fix.check_log({
      // clang-format off
      {logger::level::info,  "Trying to resolve the address of master 'mymaster' using Sentinel"   },
      {logger::level::debug, "Trying to contact Sentinel at host1:1000"                            },
      {logger::level::debug, "Executing Sentinel request at host1:1000"                            },
      {logger::level::info,  "Sentinel at host1:1000 doesn't know about master 'mymaster'"         },
      {logger::level::debug, "Trying to contact Sentinel at host2:2000"                            },
      {logger::level::debug, "Executing Sentinel request at host2:2000"                            },
      {logger::level::info,  "Sentinel at host2:2000 resolved the server address to test.host:6380"},
      // clang-format on
   });
}

}  // namespace

int main()
{
   test_success();
   test_success_replica();

   test_one_connect_error();
   test_one_request_network_error();
   test_one_master_unknown();

   return boost::report_errors();
}
