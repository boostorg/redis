/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_EXPERIMENTAL_SYNC_HPP
#define AEDIS_EXPERIMENTAL_SYNC_HPP

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

/** @brief Executes a command.
 *  @ingroup any
 *
 *  This function will block until execution completes.
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
 *  This function will block until execution completes.
 *
 *  @param conn The connection.
 *  @param req The request.
 *  @param adapter The response adapter.
 *  @throws std::system_error in case of error.
 *  @returns The number of bytes of the response.
 */
template <class Connection, class ResponseAdapter>
std::size_t
exec(Connection& conn, resp3::request const& req, ResponseAdapter adapter)
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
 *  This function will block until execution completes.
 *
 *  @param conn The connection.
 *  @param adapter The response adapter.
 *  @param ec Error code in case of error.
 *  @returns The number of bytes of the response.
 */
template <class Connection, class ResponseAdapter>
std::size_t
receive(
   Connection& conn,
   ResponseAdapter adapter,
   boost::system::error_code& ec)
{
   std::mutex mutex;
   std::condition_variable cv;
   bool ready = false;
   std::size_t res = 0;

   auto f = [&conn, &ec, &res, &mutex, &cv, &ready, adapter]()
   {
      conn.async_receive(adapter, [&cv, &mutex, &ready, &res, &ec](auto const& ecp, auto, std::size_t n) {
         std::unique_lock ul(mutex);
         ec = ecp;
         res = n;
         ready = true;
         ul.unlock();
         cv.notify_one();
      });
   };

   boost::asio::dispatch(boost::asio::bind_executor(conn.get_executor(), f));
   std::unique_lock lk(mutex);
   cv.wait(lk, [&ready]{return ready;});
   return res;
}

} // experimental
} // aedis

#endif // AEDIS_EXPERIMENTAL_SYNC_HPP
