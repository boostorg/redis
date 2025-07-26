//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/connection_logger.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/logger.hpp>

#include <boost/assert/source_location.hpp>
#include <boost/core/lightweight_test.hpp>

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>

using namespace boost::redis;
using detail::connection_logger;
using boost::system::error_code;

namespace boost::redis {

// Printing
std::ostream& operator<<(std::ostream& os, logger::level lvl)
{
   switch (lvl) {
      case logger::level::disabled: return os << "logger::level::disabled";
      case logger::level::emerg:    return os << "logger::level::emerg";
      case logger::level::alert:    return os << "logger::level::alert";
      case logger::level::crit:     return os << "logger::level::crit";
      case logger::level::err:      return os << "logger::level::err";
      case logger::level::warning:  return os << "logger::level::warning";
      case logger::level::notice:   return os << "logger::level::notice";
      case logger::level::info:     return os << "logger::level::info";
      case logger::level::debug:
         return os << "logger::level::debug";
         return os << "<unknown logger::level>";
   }
}

}  // namespace boost::redis

namespace {

// Mock logger that records the last issued message and
// the number of issued messages
struct fixture {
   std::size_t num_msgs{};
   logger::level msg_level{};
   std::string msg;
   connection_logger logger;

   explicit fixture(logger::level lvl)
   : logger({lvl, [this](logger::level lvl, std::string_view msg) {
                ++this->num_msgs;
                this->msg_level = lvl;
                this->msg = msg;
             }})
   { }
};

// log with only a message
void test_log_message()
{
   // Setup
   fixture fix{logger::level::warning};

   // Issuing a message with level > the one configured logs it
   fix.logger.log(logger::level::alert, "some message");
   BOOST_TEST_EQ(fix.num_msgs, 1u);
   BOOST_TEST_EQ(fix.msg_level, logger::level::alert);
   BOOST_TEST_EQ(fix.msg, "some message");

   // Issuing a message with level == the one configured logs it.
   // Internal buffers are cleared
   fix.logger.log(logger::level::warning, "other thing");
   BOOST_TEST_EQ(fix.num_msgs, 2u);
   BOOST_TEST_EQ(fix.msg_level, logger::level::warning);
   BOOST_TEST_EQ(fix.msg, "other thing");

   // Issuing a message with level < the one configured does not log it.
   fix.logger.log(logger::level::info, "bad");
   BOOST_TEST_EQ(fix.num_msgs, 2u);
}

// log with a message and an error code
void test_log_message_errorcode()
{
   // Setup
   fixture fix{logger::level::warning};

   // Issuing a message with level > the one configured logs it
   fix.logger.log(logger::level::alert, "Some message", error::connect_timeout);
   BOOST_TEST_EQ(fix.num_msgs, 1u);
   BOOST_TEST_EQ(fix.msg_level, logger::level::alert);
   BOOST_TEST_EQ(fix.msg, "Some message: Connect timeout. [boost.redis:18]");

   // Issuing a message with level == the one configured logs it.
   // Internal buffers are cleared.
   // Source code info is not printed
   constexpr auto loc = BOOST_CURRENT_LOCATION;
   fix.logger.log(logger::level::warning, "Other thing", error_code(error::empty_field, &loc));
   BOOST_TEST_EQ(fix.num_msgs, 2u);
   BOOST_TEST_EQ(fix.msg_level, logger::level::warning);
   BOOST_TEST_EQ(fix.msg, "Other thing: Expected field value is empty. [boost.redis:5]");

   // Issuing a message with level < the one configured does not log it.
   fix.logger.log(logger::level::info, "bad", error::expects_resp3_map);
   BOOST_TEST_EQ(fix.num_msgs, 2u);
}

}  // namespace

int main()
{
   test_log_message();
   test_log_message_errorcode();

   return boost::report_errors();
}
