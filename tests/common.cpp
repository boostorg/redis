#include "common.hpp"
#include <iostream>
#include <boost/asio/consign.hpp>

#include <boost/test/unit_test.hpp>

struct run_callback {
   std::shared_ptr<boost::redis::connection> conn;
   boost::redis::operation op;
   boost::system::error_code expected;

   void operator()(boost::system::error_code const& ec) const
   {
      std::cout << "async_run: " << ec.message() << std::endl;
      //BOOST_CHECK_EQUAL(ec, expected);
      conn->cancel(op);
   }
};

void
run(
   std::shared_ptr<boost::redis::connection> conn,
   boost::redis::config cfg,
   boost::system::error_code ec,
   boost::redis::operation op)
{
   conn->async_run(cfg, {}, run_callback{conn, op, ec});
}

