/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_SYNC_BASE_HPP
#define AEDIS_SYNC_BASE_HPP

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
template <class Executor, class Derived>
class sync_base {
public:
   /// The executor type.
   using executor_type = Executor;

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
         derived().next_layer().async_exec(req, adapter, [&sh, &res, &ec](auto const& ecp, std::size_t n) {
            std::unique_lock ul(sh.mutex);
            ec = ecp;
            res = n;
            sh.ready = true;
            ul.unlock();
            sh.cv.notify_one();
         });
      };

      boost::asio::dispatch(boost::asio::bind_executor(derived().get_executor(), f));
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
         derived().next_layer().async_receive_push(adapter, [&ec, &res, &sh](auto const& e, std::size_t n) {
            std::unique_lock ul(sh.mutex);
            ec = e;
            res = n;
            sh.ready = true;
            ul.unlock();
            sh.cv.notify_one();
         });
      };

      boost::asio::dispatch(boost::asio::bind_executor(derived().get_executor(), f));
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
   void run(endpoint ep, boost::system::error_code& ec)
   {
      sync_helper sh;
      auto f = [this, ep, &ec, &sh]()
      {
         derived().next_layer().async_run(ep, [&ec, &sh](auto const& e) {
            std::unique_lock ul(sh.mutex);
            ec = e;
            sh.ready = true;
            ul.unlock();
            sh.cv.notify_one();
         });
      };

      boost::asio::dispatch(boost::asio::bind_executor(derived().get_executor(), f));
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
   void run(endpoint ep)
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
         derived().next_layer().async_run(ep, req, adapter, [&sh, &res, &ec](auto const& ecp, std::size_t n) {
            std::unique_lock ul(sh.mutex);
            ec = ecp;
            res = n;
            sh.ready = true;
            ul.unlock();
            sh.cv.notify_one();
         });
      };

      boost::asio::dispatch(boost::asio::bind_executor(derived().get_executor(), f));
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
   template <class Operation>
   auto cancel(Operation op)
   {
      sync_helper sh;
      std::size_t res = 0;

      auto f = [this, op, &res, &sh]()
      {
         std::unique_lock ul(sh.mutex);
         res = derived().next_layer().cancel(op);
         sh.ready = true;
         ul.unlock();
         sh.cv.notify_one();
      };

      boost::asio::dispatch(boost::asio::bind_executor(derived().get_executor(), f));
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
         derived().reset_stream();
         sh.ready = true;
         ul.unlock();
         sh.cv.notify_one();
      };

      boost::asio::dispatch(boost::asio::bind_executor(derived().get_executor(), f));
      std::unique_lock lk(sh.mutex);
      sh.cv.wait(lk, [&sh]{return sh.ready;});
   }

private:
   Derived& derived() { return static_cast<Derived&>(*this); }

   struct sync_helper {
      std::mutex mutex;
      std::condition_variable cv;
      bool ready = false;
   };
};

} // aedis

#endif // AEDIS_SYNC_BASE_HPP
