/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_READ_HPP
#define BOOST_REDIS_READ_HPP

#include <boost/redis/resp3/type.hpp>
#include <boost/redis/resp3/parser.hpp>
#include <boost/redis/detail/read_ops.hpp>
#include <boost/redis/adapter/ignore.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/async_result.hpp>

namespace boost::redis::detail {

/** \brief Reads a complete response to a command sychronously.
 *
 *  This function reads a complete response to a command or a
 *  server push synchronously. For example
 *
 *  @code
 *  int resp;
 *  std::string buffer;
 *  resp3::read(socket, dynamic_buffer(buffer), adapt(resp));
 *  @endcode
 *
 *  For a complete example see examples/intro_sync.cpp. This function
 *  is implemented in terms of one or more calls to @c
 *  asio::read_until and @c asio::read functions, and is known as a @a
 *  composed @a operation. Furthermore, the implementation may read
 *  additional bytes from the stream that lie past the end of the
 *  message being read. These additional bytes are stored in the
 *  dynamic buffer, which must be preserved for subsequent reads.
 *
 *  \param stream The stream from which to read e.g. a tcp socket.
 *  \param buf Dynamic buffer (version 2).
 *  \param adapter The response adapter.
 *  \param ec If an error occurs, it will be assigned to this paramter.
 *  \returns The number of bytes that have been consumed from the dynamic buffer.
 *
 *  \remark This function calls buf.consume() in each chunk of data
 *  after it has been passed to the adapter. Users must not consume
 *  the bytes after it returns.
 */
template <
     class SyncReadStream,
     class DynamicBuffer,
     class ResponseAdapter
  >
auto
read(
   SyncReadStream& stream,
   DynamicBuffer buf,
   ResponseAdapter adapter,
   system::error_code& ec) -> std::size_t
{
   resp3::parser p;
   std::size_t n = 0;
   std::size_t consumed = 0;
   do {
      if (!p.bulk_expected()) {
	 n = asio::read_until(stream, buf, "\r\n", ec);
	 if (ec)
	    return 0;

      } else {
	 auto const s = buf.size();
	 auto const l = p.bulk_length();
	 if (s < (l + 2)) {
	    auto const to_read = l + 2 - s;
	    buf.grow(to_read);
	    n = asio::read(stream, buf.data(s, to_read), ec);
	    if (ec)
	       return 0;
	 }
      }

      auto const* data = static_cast<char const*>(buf.data(0, n).data());
      auto const res = p.consume(data, n, ec);
      if (ec)
         return 0;

      if (!p.bulk_expected()) {
         adapter(res.first, ec);
         if (ec)
            return 0;
      }

      buf.consume(res.second);
      consumed += res.second;
   } while (!p.done());

   return consumed;
}

/** \brief Reads a complete response to a command sychronously.
 *  
 *  Same as the error_code overload but throws on error.
 */
template<
   class SyncReadStream,
   class DynamicBuffer,
   class ResponseAdapter = adapter::ignore>
auto
read(
   SyncReadStream& stream,
   DynamicBuffer buf,
   ResponseAdapter adapter = ResponseAdapter{})
{
   system::error_code ec;
   auto const n = redis::detail::read(stream, buf, adapter, ec);

   if (ec)
       BOOST_THROW_EXCEPTION(system::system_error{ec});

   return n;
}

/** \brief Reads a complete response to a Redis command asynchronously.
 *
 *  This function reads a complete response to a command or a
 *  server push asynchronously. For example
 *
 *  @code
 *  std::string buffer;
 *  std::set<std::string> resp;
 *  co_await resp3::async_read(socket, dynamic_buffer(buffer), adapt(resp));
 *  @endcode
 *
 *  For a complete example see examples/transaction.cpp. This function
 *  is implemented in terms of one or more calls to @c
 *  asio::async_read_until and @c asio::async_read functions, and is
 *  known as a @a composed @a operation.  Furthermore, the
 *  implementation may read additional bytes from the stream that lie
 *  past the end of the message being read. These additional bytes are
 *  stored in the dynamic buffer, which must be preserved for
 *  subsequent reads.
 *
 *  \param stream The stream from which to read e.g. a tcp socket.
 *  \param buffer Dynamic buffer (version 2).
 *  \param adapter The response adapter.
 *  \param token The completion token.
 *
 *  The completion handler will receive as a parameter the total
 *  number of bytes transferred from the stream and must have the
 *  following signature
 *
 *  @code
 *  void(system::error_code, std::size_t);
 *  @endcode
 *
 *  \remark This function calls buf.consume() in each chunk of data
 *  after it has been passed to the adapter. Users must not consume
 *  the bytes after it returns.
 */
template <
   class AsyncReadStream,
   class DynamicBuffer,
   class ResponseAdapter = adapter::ignore,
   class CompletionToken = asio::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read(
   AsyncReadStream& stream,
   DynamicBuffer buffer,
   ResponseAdapter adapter = ResponseAdapter{},
   CompletionToken&& token =
      asio::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return asio::async_compose
      < CompletionToken
      , void(system::error_code, std::size_t)
      >(detail::parse_op<AsyncReadStream, DynamicBuffer, ResponseAdapter> {stream, buffer, adapter},
        token,
        stream);
}

} // boost::redis::detail

#endif // BOOST_REDIS_READ_HPP
