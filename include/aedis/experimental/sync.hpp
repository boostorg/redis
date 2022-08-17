/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_EXPERIMENTAL_SYNC_HPP
#define AEDIS_EXPERIMENTAL_SYNC_HPP

#include <aedis/adapt.hpp>
#include <aedis/connection.hpp>
#include <aedis/resp3/request.hpp>

namespace aedis {
namespace experimental {
namespace detail {

struct sync {
   std::mutex mutex;
   std::condition_variable cv;
   bool ready = false;
};

} // detail

/** @brief Executes a request.
 *  @ingroup any
 *
 *  @remark This function will block until execution completes. It
 *  assumes the connection is running on a different thread.
 *
 *  @param conn The connection.
 *  @param req The request.
 *  @param adapter The response adapter.
 *  @param ec Error code in case of error.
 *  @returns The number of bytes of the response.
 */
template <class Connection, class ResponseAdapter>
std::size_t
exec(
   Connection& conn,
   resp3::request const& req,
   ResponseAdapter adapter,
   boost::system::error_code& ec)
{
   detail::sync sh;
   std::size_t res = 0;

   auto f = [&conn, &ec, &res, &sh, &req, adapter]()
   {
      conn.async_exec(req, adapter, [&sh, &res, &ec](auto const& ecp, std::size_t n) {
         std::unique_lock ul(sh.mutex);
         ec = ecp;
         res = n;
         sh.ready = true;
         ul.unlock();
         sh.cv.notify_one();
      });
   };

   boost::asio::dispatch(boost::asio::bind_executor(conn.get_executor(), f));
   std::unique_lock lk(sh.mutex);
   sh.cv.wait(lk, [&sh]{return sh.ready;});
   return res;
}

/** @brief Executes a command.
 *  @ingroup any
 *
 *  @remark This function will block until execution completes. It
 *  assumes the connection is running on a different thread.
 *
 *  @param conn The connection.
 *  @param req The request.
 *  @param adapter The response adapter.
 *  @throws std::system_error in case of error.
 *  @returns The number of bytes of the response.
 */
template <
   class Connection,
   class ResponseAdapter = aedis::detail::response_traits<void>::adapter_type>
std::size_t
exec(
   Connection& conn,
   resp3::request const& req,
   ResponseAdapter adapter = aedis::adapt())
{
   boost::system::error_code ec;
   auto const res = exec(conn, req, adapter, ec);
   if (ec)
      throw std::system_error(ec);
   return res;
}

/** @brief Receives server pushes synchronusly.
 *  @ingroup any
 *
 *  @remark This function will block until execution completes. It
 *  assumes the connection is running on a different thread.
 *
 *  @param conn The connection.
 *  @param adapter The response adapter.
 *  @param ec Error code in case of error.
 *  @returns The number of bytes of the response.
 */
template <class Connection, class ResponseAdapter>
auto receive_push(
   Connection& conn,
   ResponseAdapter adapter,
   boost::system::error_code& ec)
{
   std::mutex mutex;
   std::condition_variable cv;
   bool ready = false;
   std::size_t n = 0;

   auto f = [&conn, &ec, &mutex, &cv, &n, &ready, adapter]()
   {
      conn.async_receive_push(adapter, [&cv, &mutex, &n, &ready, &ec](auto const& ecp, std::size_t evp) {
         std::unique_lock ul(mutex);
         ec = ecp;
         n = evp;
         ready = true;
         ul.unlock();
         cv.notify_one();
      });
   };

   boost::asio::dispatch(boost::asio::bind_executor(conn.get_executor(), f));
   std::unique_lock lk(mutex);
   cv.wait(lk, [&ready]{return ready;});
   return n;
}

/** @brief Receives server pushes synchronusly.
 *  @ingroup any
 *
 *  @remark This function will block until execution completes. It
 *  assumes the connection is running on a different thread.
 *
 *  @param conn The connection.
 *  @param adapter The response adapter.
 *  @throws std::system_error in case of error.
 *  @returns The number of bytes of the response.
 */
template <
   class Connection,
   class ResponseAdapter = aedis::detail::response_traits<void>::adapter_type>
auto receive_push(
   Connection& conn,
   ResponseAdapter adapter = aedis::adapt())
{
   boost::system::error_code ec;
   auto const res = receive_push(conn, adapter, ec);
   if (ec)
      throw std::system_error(ec);
   return res;
}

/** @brief Receives events
 *  @ingroup any
 *
 *  @remark This function will block until execution completes. It
 *  assumes the connection is running on a different thread.
 *
 *  @param conn The connection.
 *  @param ec Error code in case of error.
 *  @returns The event received.
 */
template <class Connection>
auto receive_event(Connection& conn, boost::system::error_code& ec)
{
   using event_type = typename Connection::event;
   std::mutex mutex;
   std::condition_variable cv;
   bool ready = false;
   event_type ev = event_type::invalid;

   auto f = [&conn, &ec, &ev, &mutex, &cv, &ready]()
   {
      conn.async_receive_event([&cv, &mutex, &ready, &ev, &ec](auto const& ecp, event_type evp) {
         std::unique_lock ul(mutex);
         ec = ecp;
         ev = evp;
         ready = true;
         ul.unlock();
         cv.notify_one();
      });
   };

   boost::asio::dispatch(boost::asio::bind_executor(conn.get_executor(), f));
   std::unique_lock lk(mutex);
   cv.wait(lk, [&ready]{return ready;});
   return ev;
}

/** @brief Receives events
 *  @ingroup any
 *
 *  @remark This function will block until execution completes. It
 *  assumes the connection is running on a different thread.
 *
 *  @param conn The connection.
 *  @throws std::system_error in case of error.
 *  @returns The event received.
 */
template <class Connection>
auto receive_event(Connection& conn)
{
   boost::system::error_code ec;
   auto const res = receive_event(conn, ec);
   if (ec)
      throw std::system_error(ec);
   return res;
}

} // experimental
} // aedis

#endif // AEDIS_EXPERIMENTAL_SYNC_HPP
