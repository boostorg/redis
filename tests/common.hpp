#pragma once

#include <boost/asio.hpp>
#include <chrono>

namespace net = boost::asio;
using endpoints = net::ip::tcp::resolver::results_type;

auto
resolve(
   std::string const& host = "127.0.0.1",
   std::string const& port = "6379") -> endpoints
{
   net::io_context ioc;
   net::ip::tcp::resolver resv{ioc};
   return resv.resolve(host, port);
}
