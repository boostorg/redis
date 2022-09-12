/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_SYNC_HPP
#define AEDIS_SYNC_HPP

#include <condition_variable>
#include <aedis/resp3/request.hpp>

namespace aedis {

/** @brief A high level synchronous connection to Redis.
 *  @ingroup any
 *
 *  This class keeps a healthy and thread safe connection to the Redis
 *  instance where commands can be sent at any time. For more details,
 *  please see the documentation of each individual function.
 *
 */
template <class Connection>
class sync {
public:
   /// Config options from the underlying connection.
   using config = typename Connection::config;

   /// Operation options from the underlying connection.
   using operation = typename Connection::operation;

   /// The executor type of the underlysing connection.
   using executor_type = typename Connection::executor_type;

   /** @brief Constructor
    *  
    *  @param ex Executor
    *  @param cfg Config options.
    */
   explicit sync(executor_type ex, config cfg = config{}) : conn_{ex, cfg} { }

   /** @brief Constructor
    *  
    *  @param ex The io_context.
    *  @param cfg Config options.
    */
   explicit sync(boost::asio::io_context& ioc, config cfg = config{})
   : sync(ioc, std::move(cfg))
   { }

   /** @brief Calls `async_exec` from the underlying connection object.
    *
    *  Calls `async_exec` from the underlying connection object and
    *  waits for its completion.
    *
    *  @param req The request.
    *  @param adapter The response adapter.
    *  @param ec Error code in case of error.
    *  @returns The number of bytes of the response.
    */
   template <class ResponseAdapter>
   auto
   exec(resp3::request const& req, ResponseAdapter adapter, boost::system::error_code& ec)
   {
      sync_helper sh;
      std::size_t res = 0;

      auto f = [this, &ec, &res, &sh, &req, adapter]()
      {
         conn_.async_exec(req, adapter, [&sh, &res, &ec](auto const& ecp, std::size_t n) {
            std::unique_lock ul(sh.mutex);
            ec = ecp;
            res = n;
            sh.ready = true;
            ul.unlock();
            sh.cv.notify_one();
         });
      };

      boost::asio::dispatch(boost::asio::bind_executor(conn_.get_executor(), f));
      std::unique_lock lk(sh.mutex);
      sh.cv.wait(lk, [&sh]{return sh.ready;});
      return res;
   }

   /** @brief Calls `async_exec` from the underlying connection object.
    *
    *  Calls `async_exec` from the underlying connection object and
    *  waits for its completion.
    *
    *  @param req The request.
    *  @param adapter The response adapter.
    *  @throws std::system_error in case of error.
    *  @returns The number of bytes of the response.
    */
   template <class ResponseAdapter = detail::response_traits<void>::adapter_type>
   auto exec(resp3::request const& req, ResponseAdapter adapter = aedis::adapt())
   {
      boost::system::error_code ec;
      auto const res = exec(req, adapter, ec);
      if (ec)
         throw std::system_error(ec);
      return res;
   }

   /** @brief Calls `async_receive_push` from the underlying connection object.
    *
    *  Calls `async_receive_push` from the underlying connection
    *  object and waits for its completion.
    *
    *  @param adapter The response adapter.
    *  @param ec Error code in case of error.
    *  @returns The number of bytes received.
    */
   template <class ResponseAdapter>
   auto receive_push(ResponseAdapter adapter, boost::system::error_code& ec)
   {
      sync_helper sh;
      std::size_t res = 0;

      auto f = [this, &ec, &res, &sh, adapter]()
      {
         conn_.async_receive_push(adapter, [&ec, &res, &sh](auto const& e, std::size_t n) {
            std::unique_lock ul(sh.mutex);
            ec = e;
            res = n;
            sh.ready = true;
            ul.unlock();
            sh.cv.notify_one();
         });
      };

      boost::asio::dispatch(boost::asio::bind_executor(conn_.get_executor(), f));
      std::unique_lock lk(sh.mutex);
      sh.cv.wait(lk, [&sh]{return sh.ready;});
      return res;
   }

   /** @brief Calls `async_receive_push` from the underlying connection object.
    *
    *  Calls `async_receive_push` from the underlying connection
    *  object and waits for its completion.
    *
    *  @param adapter The response adapter.
    *  @throws std::system_error in case of error.
    *  @returns The number of bytes received.
    */
   template <class ResponseAdapter = aedis::detail::response_traits<void>::adapter_type>
   auto receive_push(ResponseAdapter adapter = aedis::adapt())
   {
      boost::system::error_code ec;
      auto const res = receive_push(adapter, ec);
      if (ec)
         throw std::system_error(ec);
      return res;
   }

   /** @brief Calls \c async_run from the underlying connection.
    *
    *  Calls `async_run` from the underlying connection objects and
    *  waits for its completion.
    *
    *  @param ep The Redis server endpoint.
    *  @param ec Error code.
    */
   void run(endpoint& ep, boost::system::error_code& ec)
   {
      sync_helper sh;
      auto f = [this, &ep, &ec, &sh]()
      {
         conn_.async_run(ep, [&ec, &sh](auto const& e) {
            std::unique_lock ul(sh.mutex);
            ec = e;
            sh.ready = true;
            ul.unlock();
            sh.cv.notify_one();
         });
      };

      boost::asio::dispatch(boost::asio::bind_executor(conn_.get_executor(), f));
      std::unique_lock lk(sh.mutex);
      sh.cv.wait(lk, [&sh]{return sh.ready;});
   }

   /** @brief Calls \c async_run from the underlying connection.
    *
    *  Calls `async_run` from the underlying connection objects and
    *  waits for its completion.
    *
    *  @param ep The Redis server endpoint.
    *  @throws std::system_error.
    */
   void run(endpoint& ep)
   {
      boost::system::error_code ec;
      run(std::move(ep), ec);
      if (ec)
         throw std::system_error(ec);
   }

   /** @brief Calls \c async_run from the underlying connection.
    *
    *  Calls `async_run` from the underlying connection objects and
    *  waits for its completion.
    *
    *  @param ep The Redis server endpoint.
    *  @param req The request. 
    *  @param adapter The response adapter. 
    *  @param ec Error code in case of error.
    */
   template <class ResponseAdapter>
   auto run(endpoint& ep, resp3::request const& req, ResponseAdapter adapter, boost::system::error_code& ec)
   {
      sync_helper sh;
      std::size_t res = 0;

      auto f = [this, ep, &ec, &res, &sh, &req, adapter]() mutable
      {
         conn_.async_run(ep, req, adapter, [&sh, &res, &ec](auto const& ecp, std::size_t n) {
            std::unique_lock ul(sh.mutex);
            ec = ecp;
            res = n;
            sh.ready = true;
            ul.unlock();
            sh.cv.notify_one();
         });
      };

      boost::asio::dispatch(boost::asio::bind_executor(conn_.get_executor(), f));
      std::unique_lock lk(sh.mutex);
      sh.cv.wait(lk, [&sh]{return sh.ready;});
      return res;
   }

   /** @brief Calls \c async_run from the underlying connection.
    *
    *  Calls `async_run` from the underlying connection objects and
    *  waits for its completion.
    *
    *  @param ep The Redis server endpoint.
    *  @param req The request. 
    *  @param adapter The response adapter. 
    *  @throws std::system_error.
    */
   template <class ResponseAdapter = detail::response_traits<void>::adapter_type>
   auto run(endpoint& ep, resp3::request const& req, ResponseAdapter adapter = aedis::adapt())
   {
      boost::system::error_code ec;
      auto const res = run(ep, req, adapter, ec);
      if (ec)
         throw std::system_error(ec);
      return res;
   }

   /** @brief Calls `cancel` in the underlying connection object.
    *
    *  @param op The operation to cancel.
    *  @returns The number of operations canceled.
    */
   template <class ResponseAdapter>
   auto cancel(operation op)
   {
      sync_helper sh;
      std::size_t res = 0;

      auto f = [this, op, &res, &sh]()
      {
         std::unique_lock ul(sh.mutex);
         res = conn_.cancel(op);
         sh.ready = true;
         ul.unlock();
         sh.cv.notify_one();
      };

      boost::asio::dispatch(boost::asio::bind_executor(conn_.get_executor(), f));
      std::unique_lock lk(sh.mutex);
      sh.cv.wait(lk, [&sh]{return sh.ready;});
      return res;
   }

   /** @brief Calls `reset` in the underlying connection object.
    *
    *  @param op The operation to cancel.
    *  @returns The number of operations canceled.
    */
   void reset_stream()
   {
      sync_helper sh;

      auto f = [this, &sh]()
      {
         std::unique_lock ul(sh.mutex);
         conn_.reset_stream();
         sh.ready = true;
         ul.unlock();
         sh.cv.notify_one();
      };

      boost::asio::dispatch(boost::asio::bind_executor(conn_.get_executor(), f));
      std::unique_lock lk(sh.mutex);
      sh.cv.wait(lk, [&sh]{return sh.ready;});
   }

private:
   struct sync_helper {
      std::mutex mutex;
      std::condition_variable cv;
      bool ready = false;
   };

   Connection conn_;
};

} // aedis

#endif // AEDIS_SYNC_HPP
