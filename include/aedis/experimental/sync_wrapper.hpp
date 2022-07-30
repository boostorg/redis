/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_EXPERIMENTAL_SYNC_WRAPPER_HPP
#define AEDIS_EXPERIMENTAL_SYNC_WRAPPER_HPP

#include <mutex>
#include <thread>
#include <memory>

#include <boost/asio/io_context.hpp>
#include <aedis/connection.hpp>
#include <aedis/resp3/request.hpp>

namespace aedis {
namespace experimental {
namespace detail {

#include <boost/asio/yield.hpp>
template <class Connection>
struct failover_op {
   std::shared_ptr<Connection> db;
   boost::string_view host;
   boost::string_view port;
   //typename Connection::timer_type timer{db->get_executor()};
   boost::asio::coroutine coro{};

   template <class Self>
   void operator()(Self& self, boost::system::error_code ec = {})
   {
      reenter (coro)
      {
         BOOST_ASSERT(db != nullptr);
         yield db->async_run(host, port, std::move(self));
         //timer.expires_at(std::chrono::seconds{1});
         self.complete(ec);
      }
   }
};
#include <boost/asio/unyield.hpp>

template <
   class Connection,
   class CompletionToken = boost::asio::default_completion_token_t<typename Connection::executor_type>
   >
auto async_failover(
   std::shared_ptr<Connection> db,
   boost::string_view host,
   boost::string_view port,
   CompletionToken&& token =
      boost::asio::default_completion_token_t<typename Connection::executor_type>{})
{
   return boost::asio::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(detail::failover_op<Connection> {db, host, port}, token, db->get_executor());
}

} // detail

/** @brief Blocking wrapper for the conenction class.
 *  @ingroup any
 *  
 *  This class offers a synchronous API on top of the connection
 *  class.
 */
template <class Connection>
class sync_wrapper {
private:
   boost::asio::io_context ioc{1};
   std::shared_ptr<Connection> db;
   std::thread thread;

public:
   // Sync wrapper configuration parameters.
   struct config {
      /// Time waited by trying a reconnection.
      std::chrono::seconds reconnect_wait_time{2};
   };

   /// Constructor
   sync_wrapper(config = config{})
   : db{std::make_shared<Connection>(ioc)}
   { }

   // Destructor
   ~sync_wrapper()
   {
      ioc.stop();
      thread.join();
   }

   void run(boost::string_view host, boost::string_view port)
   {
      thread = std::thread{[this, host, port]() {
         detail::async_failover(db, host, port, boost::asio::detached);
         ioc.run();
      }};
   }

   /** @brief Executes a command.
    *
    *  This function will block until execution completes.
    *
    *  @param req The request.
    *  @param adapter The response adapter.
    *  @param ec Error code in case of error.
    *  @returns The number of bytes of the response.
    */
   template <class ResponseAdapter>
   std::size_t
   exec(resp3::request const& req, ResponseAdapter adapter, boost::system::error_code& ec)
   {
      std::mutex m;
      std::condition_variable cv;
      bool ready = false;
      std::size_t res = 0;

      auto f = [this, &ec, &res, &m, &cv, &ready, &req, adapter]()
      {
         std::lock_guard lk(m);
         db->async_exec(req, adapter, [&cv, &ready, &res, &ec](auto const& ecp, std::size_t n) {
            ec = ecp;
            res = n;
            ready = true;
            cv.notify_one();
         });
      };

      boost::asio::dispatch(boost::asio::bind_executor(ioc, f));
      std::unique_lock lk(m);
      cv.wait(lk, [&ready]{return ready;});
      return res;
   }

   /** @brief Executes a command.
    *
    *  This function will block until execution completes.
    *
    *  @param req The request.
    *  @param adapter The response adapter.
    *  @throws std::system_error in case of error.
    *  @returns The number of bytes of the response.
    */
   template <class ResponseAdapter>
   std::size_t exec(resp3::request const& req, ResponseAdapter adapter)
   {
      boost::system::error_code ec;
      auto const res = exec(req, adapter, ec);
      if (ec)
         throw std::system_error(ec);
      return res;
   }
};

#endif

} // experimental
} // aedis
