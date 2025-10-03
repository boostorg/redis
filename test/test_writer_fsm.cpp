//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/writer_fsm.hpp>
#include <boost/redis/request.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/assert.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "sansio_utils.hpp"

#include <cstddef>
#include <memory>
#include <ostream>
#include <utility>

using namespace boost::redis;
namespace asio = boost::asio;
using detail::writer_fsm;
using detail::multiplexer;
using detail::writer_action_type;
using detail::consume_result;
using detail::writer_action;
using boost::system::error_code;
using boost::asio::cancellation_type_t;

// Operators
static const char* to_string(writer_action_type value)
{
   switch (value) {
      case writer_action_type::done:  return "writer_action_type::done";
      case writer_action_type::write: return "writer_action_type::write";
      case writer_action_type::wait:  return "writer_action_type::wait";
      default:                        return "<unknown writer_action_type>";
   }
}

namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, writer_action_type type)
{
   os << to_string(type);
   return os;
}

bool operator==(const writer_action& lhs, const writer_action& rhs) noexcept
{
   return lhs.type == rhs.type && lhs.ec == rhs.ec;
}

std::ostream& operator<<(std::ostream& os, const writer_action& act)
{
   os << "writer_action{ .type=" << act.type;
   if (act.type == writer_action_type::done)
      os << ", .error=" << act.ec;
   return os << " }";
}

}  // namespace boost::redis::detail

namespace {

// A helper to create a request and its associated elem
struct test_elem {
   request req;
   bool done{false};
   std::shared_ptr<multiplexer::elem> elm;

   test_elem()
   {
      // Empty requests are not valid. The request needs to be populated before creating the element
      req.push("get", "mykey");
      elm = std::make_shared<multiplexer::elem>(req, any_adapter{});

      elm->set_done_callback([this] {
         done = true;
      });
   }
};

// A single request is written, then we wait and repeat
void test_single_request()
{
   // Setup
   multiplexer mpx;
   test_elem elem;
   // TODO
}

}  // namespace

int main()
{
   test_single_request();

   return boost::report_errors();
}
