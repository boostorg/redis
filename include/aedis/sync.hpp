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
   using event = typename Connection::event;
   using config = typename Connection::config;

   /** @brief Constructor
    *  
    *  @param ex Executor
    *  @param cfg Config options.
    */
   template <class Executor>
   sync(Executor ex, config cfg = config{}) : conn_{ex, cfg} { }

   /** @brief Executes a request synchronously.
    *
    *  The functions calls `connection::async_exec` and waits
    *  for its completion.
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

   /** @brief Executes a command synchronously
    *
    *  The functions calls `connection::async_exec` and waits for its
    *  completion.
    *
    *  @param req The request.
    *  @param adapter The response adapter.
    *  @throws std::system_error in case of error.
    *  @returns The number of bytes of the response.
    */
   template <class ResponseAdapter = detail::response_traits<void>::adapter_type>
   std::size_t exec(resp3::request const& req, ResponseAdapter adapter = aedis::adapt())
   {
      boost::system::error_code ec;
      auto const res = exec(req, adapter, ec);
      if (ec)
         throw std::system_error(ec);
      return res;
   }

   /** @brief Receives server pushes synchronusly.
    *
    *  The functions calls `connection::async_receive_push` and
    *  waits for its completion.
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

   /** @brief Receives server pushes synchronusly.
    *
    *  The functions calls `connection::async_receive_push` and
    *  waits for its completion.
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

   /** @brief Receives events synchronously.
    *
    *  The functions calls `connection::async_receive_event` and
    *  waits for its completion.
    *
    *  @param ec Error code in case of error.
    *  @returns The event received.
    */
   auto receive_event(boost::system::error_code& ec)
   {
      sync_helper sh;
      auto res = event::invalid;

      auto f = [this, &ec, &res, &sh]()
      {
         conn_.async_receive_event([&ec, &res, &sh](auto const& ecp, event ev) {
            std::unique_lock ul(sh.mutex);
            ec = ecp;
            res = ev;
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

   /** @brief Receives events synchronously
    *
    *  The functions calls `connection::async_receive_event` and
    *  waits for its completion.
    *
    *  @throws std::system_error in case of error.
    *  @returns The event received.
    */
   auto receive_event()
   {
      boost::system::error_code ec;
      auto const res = receive_event(ec);
      if (ec)
         throw std::system_error(ec);
      return res;
   }

   /** @brief Calls \c async_run from the underlying connection.
    *
    *  The functions calls `connection::async_run` and waits for its
    *  completion.
    *
    *  @param ec Error code.
    */
   void run(boost::system::error_code& ec)
   {
      sync_helper sh;
      auto f = [this, &ec, &sh]()
      {
         conn_.async_run([&ec, &sh](auto const& e) {
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
    *  The functions calls `connection::async_run` and waits for its
    *  completion.
    *
    *  @throws std::system_error.
    */
   void run()
   {
      boost::system::error_code ec;
      run(ec);
      if (ec)
         throw std::system_error(ec);
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
