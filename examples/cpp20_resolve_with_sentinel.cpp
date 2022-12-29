/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <aedis.hpp>

#include "common/common.hpp"

namespace net = boost::asio;
namespace resp3 = aedis::resp3;
using namespace net::experimental::awaitable_operators;
using endpoints = net::ip::tcp::resolver::results_type;
using aedis::adapt;

auto redir(boost::system::error_code& ec)
   { return net::redirect_error(net::use_awaitable, ec); }

struct address {
   std::string host;
   std::string port;
};

// For more info see
// - https://redis.io/docs/manual/sentinel.
// - https://redis.io/docs/reference/sentinel-clients.
auto resolve_master_address(std::vector<address> const& endpoints) -> net::awaitable<address>
{
   resp3::request req;
   req.push("SENTINEL", "get-master-addr-by-name", "mymaster");
   req.push("QUIT");

   auto conn = std::make_shared<connection>(co_await net::this_coro::executor);

   std::tuple<std::optional<std::array<std::string, 2>>, aedis::ignore> addr;
   for (auto ep : endpoints) {
      boost::system::error_code ec;
      co_await connect(conn, ep.host, ep.port);
      co_await (conn->async_run() && conn->async_exec(req, adapt(addr), redir(ec)));
      conn->reset_stream();
      if (std::get<0>(addr))
         co_return address{std::get<0>(addr).value().at(0), std::get<0>(addr).value().at(1)};
   }

   co_return address{};
}

auto async_main() -> net::awaitable<void>
{
   // A list of sentinel addresses from which only one is responsive
   // to simulate sentinels that are down.
   std::vector<address> const endpoints
   { {"foo", "26379"}
   , {"bar", "26379"}
   , {"127.0.0.1", "26379"}
   };

   auto const ep = co_await resolve_master_address(endpoints);

   std::clog
      << "Host: " << ep.host << "\n"
      << "Port: " << ep.port << "\n"
      << std::flush;
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
