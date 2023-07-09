/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_READ_HPP
#define BOOST_REDIS_READ_HPP

#include <boost/redis/resp3/type.hpp>
#include <boost/redis/resp3/parser.hpp>
#include <boost/redis/adapter/ignore.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/redis/detail/helper.hpp>

#include <string_view>
#include <limits>

namespace boost::redis::detail {

template <class DynamicBuffer>
std::string_view buffer_view(DynamicBuffer buf) noexcept
{
   char const* start = static_cast<char const*>(buf.data(0, buf.size()).data());
   return std::string_view{start, std::size(buf)};
}

template <
   class AsyncReadStream,
   class DynamicBuffer,
   class ResponseAdapter>
class parse_op {
private:
   AsyncReadStream& stream_;
   DynamicBuffer buf_;
   resp3::parser parser_;
   ResponseAdapter adapter_;
   std::size_t tmp_ = 0;
   resp3::parser::result res_;
   asio::coroutine coro_{};

   static std::size_t const growth = 1024;

public:
   parse_op(AsyncReadStream& stream, DynamicBuffer buf, ResponseAdapter adapter)
   : stream_ {stream}
   , buf_ {std::move(buf)}
   , adapter_ {std::move(adapter)}
   { }

   template <class Self>
   void operator()( Self& self
                  , system::error_code ec = {}
                  , std::size_t n = 0)
   {
      BOOST_ASIO_CORO_REENTER (coro_) for (;;) {

         res_ = parser_.consume(buffer_view(buf_), ec);
         if (ec)
            return self.complete(ec, 0);

         if (!res_.has_value()) {
            tmp_ = buf_.size();
            buf_.grow(parser_.get_suggested_buffer_growth(growth));

            BOOST_ASIO_CORO_YIELD
            stream_.async_read_some(
               buf_.data(tmp_, parser_.get_suggested_buffer_growth(growth)),
               std::move(self));
            BOOST_REDIS_CHECK_OP1(;);

            buf_.shrink(buf_.size() - tmp_ - n);
            continue;
         }

         adapter_(res_.value(), ec);
         if (ec)
            return self.complete(ec, 0);

         if (parser_.done()) {
            self.complete({}, parser_.get_consumed());
            return;
         }
      }
   }
};

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
   static std::size_t const growth = 1024;

   resp3::parser parser;
   while (!parser.done()) {
      auto const res = parser.consume(detail::buffer_view(buf), ec);
      if (ec)
         return 0UL;

      if (!res.has_value()) {
         auto const size_before = buf.size();
         buf.grow(parser.get_suggested_buffer_growth(growth));
         auto const n =
            stream.read_some(
               buf.data(size_before, parser.get_suggested_buffer_growth(growth)),
               ec);
         if (ec)
            return 0UL;

         buf.shrink(buf.size() - size_before - n);
         continue;
      }

      adapter(res.value(), ec);
      if (ec)
         return 0UL;
   }

   return parser.get_consumed();
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
      >(parse_op<AsyncReadStream, DynamicBuffer, ResponseAdapter> {stream, buffer, adapter},
        token,
        stream);
}

} // boost::redis::detail

#endif // BOOST_REDIS_READ_HPP
