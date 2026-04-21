#include <boost/redis/config.hpp>
#include <boost/redis/ignore.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/core/lightweight_test.hpp>

#include "asio_common.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace net = boost::asio;

struct run_callback {
   std::shared_ptr<boost::redis::connection> conn;
   boost::redis::operation op;
   boost::system::error_code expected;

   void operator()(boost::system::error_code const& ec) const
   {
      std::cout << "async_run: " << ec.message() << std::endl;
      conn->cancel(op);
   }
};

void run(
   std::shared_ptr<boost::redis::connection> conn,
   boost::redis::config cfg,
   boost::system::error_code ec,
   boost::redis::operation op)
{
   conn->async_run(cfg, run_callback{conn, op, ec});
}

#ifdef BOOST_ASIO_HAS_CO_AWAIT
void run_coroutine_test(net::awaitable<void> op, std::chrono::steady_clock::duration timeout)
{
   net::io_context ioc;
   bool finished = false;
   net::co_spawn(ioc, std::move(op), [&finished](std::exception_ptr p) {
      if (p)
         std::rethrow_exception(p);
      finished = true;
   });
   ioc.run_for(timeout);
   if (!finished)
      throw std::runtime_error("Coroutine test did not finish");
}
#endif  // BOOST_ASIO_HAS_CO_AWAIT

void create_user(std::string_view port, std::string_view username, std::string_view password)
{
   // Setup
   net::io_context ioc;
   boost::redis::connection conn{ioc};

   boost::redis::config cfg;
   cfg.addr.port = port;

   // Enable the user and grant them permissions on everything
   boost::redis::request req;
   req.push("ACL", "SETUSER", username, "on", ">" + std::string(password), "~*", "&*", "+@all");

   bool run_finished = false, exec_finished = false;

   conn.async_run(cfg, [&](boost::system::error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   conn.async_exec(req, boost::redis::ignore, [&](boost::system::error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, boost::system::error_code());
      conn.cancel();
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
   BOOST_TEST(exec_finished);
}
