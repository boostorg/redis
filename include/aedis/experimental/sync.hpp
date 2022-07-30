/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_EXPERIMENTAL_SYNC_HPP
#define AEDIS_EXPERIMENTAL_SYNC_HPP

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
   Connection* db;
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
   Connection& db,
   boost::string_view host,
   boost::string_view port,
   CompletionToken&& token =
      boost::asio::default_completion_token_t<typename Connection::executor_type>{})
{
   return boost::asio::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(detail::failover_op<Connection> {&db, host, port}, token, db.get_executor());
}

} // detail

// Sync wrapper configuration parameters.
struct failover_config {
   /// Redis server address.
   boost::string_view host = "127.0.0.1";

   /// Redis server port.
   boost::string_view port = "6379";

   /// Time waited before trying a reconnection.
   std::chrono::seconds reconnect_wait_time{2};
};

/** @brief Synchronous wrapper over the conenction class.
 *  @ingroup any
 *  
 *  This class offers a synchronous API on top of the connection
 *  class.
 *
 *  @tparam Connection The connection class. Its executor will be
 *  rebound to an internal executor.
 */
template <class Connection>
class sync {
private:
   using executor_type = boost::asio::io_context::executor_type;
   using stream_type = typename Connection::next_layer_type;
   using stream_rebind_type = typename stream_type::rebind_executor<executor_type>;
   using stream_rebound_type = typename stream_rebind_type::other;
   using connection_rebind_type = typename Connection::rebind<stream_rebound_type>;
   using connection_rebound_type = typename connection_rebind_type::other;
   using connection_config_type = typename connection_rebound_type::config;
   boost::asio::io_context ioc_{1};
   connection_rebound_type db_;
   std::thread thread_;

public:
   /// Constructor
   sync(failover_config cfg = failover_config{}, connection_config_type conn_cfg = connection_config_type{})
   : db_{ioc_, conn_cfg}
   {
      thread_ = std::thread{[this, cfg]() {
         detail::async_failover(db_, cfg.host, cfg.port, boost::asio::detached);
         ioc_.run();
      }};
   }

   // Destructor
   ~sync()
   {
      ioc_.stop();
      thread_.join();
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
      std::mutex mutex;
      std::condition_variable cv;
      bool ready = false;
      std::size_t res = 0;

      auto f = [this, &ec, &res, &mutex, &cv, &ready, &req, adapter]()
      {
         std::lock_guard lk(mutex);
         db_.async_exec(req, adapter, [&cv, &ready, &res, &ec](auto const& ecp, std::size_t n) {
            ec = ecp;
            res = n;
            ready = true;
            cv.notify_one();
         });
      };

      boost::asio::dispatch(boost::asio::bind_executor(ioc_, f));
      std::unique_lock lk(mutex);
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
