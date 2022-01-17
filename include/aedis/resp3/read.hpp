/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/net.hpp>

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/adapt.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/detail/read_ops.hpp>
#include <aedis/resp3/detail/response_traits.hpp>

#include <boost/asio/yield.hpp>

namespace aedis {
namespace resp3 {

/** \brief Read the response to a command sychronously.
 *  \ingroup functions
 *
 *  This function has to be called once for each command in the
 *  request until the whole request has been read.
 *
 *  \param stream The stream from which to read.
 *  \param buf Auxiliary read buffer, usually a `std::string`.
 *  \param adapter The response adapter, see adapt.
 *  \param ec Error if any.
 *  \returns The number of bytes that have been consumed from the
 *  auxiliary buffer.
 */
template <
  class SyncReadStream,
  class DynamicBuffer,
  class ResponseAdapter
  >
std::size_t
read(
   SyncReadStream& stream,
   DynamicBuffer buf,
   ResponseAdapter adapter,
   boost::system::error_code& ec)
{
   detail::parser p {adapter};
   std::size_t n = 0;
   std::size_t consumed = 0;
   do {
      if (p.bulk() == type::invalid) {
	 n = net::read_until(stream, buf, "\r\n", ec);
	 if (ec)
	    return 0;

	 if (n < 3) {
            ec = error::unexpected_read_size;
            return 0;
         }
      } else {
	 auto const s = std::size(buf);
	 auto const l = p.bulk_length();
	 if (s < (l + 2)) {
	    auto const to_read = l + 2 - s;
	    buf.grow(to_read);
	    n = net::read(stream, buf.data(s, to_read), ec);
	    if (ec)
	       return 0;

            if (n < to_read) {
               ec = error::unexpected_read_size;
               return 0;
            }
	 }
      }

      std::error_code ec;
      auto const* data = (char const*) buf.data(0, n).data();
      n = p.advance(data, n, ec);
      if (ec)
         return 0;

      buf.consume(n);
      consumed += n;
   } while (!p.done());

   return consumed;
}

/** \brief Reads the reponse to a command.
 *  \ingroup functions
 *  
 *  This function has to be called once for each command in the
 *  request until the whole request has been read.
 *
 *  \param stream The stream from which to read.
 *  \param buf Auxiliary read buffer, usually a `std::string`.
 *  \param adapter The response adapter, see adapt.
 *  \returns The number of bytes that have been consumed from the
 *  auxiliary buffer.
 */
template<
   class SyncReadStream,
   class DynamicBuffer,
   class ResponseAdapter = detail::response_traits<void>::adapter_type>
std::size_t
read(
   SyncReadStream& stream,
   DynamicBuffer buf,
   ResponseAdapter adapter = adapt())
{
   boost::system::error_code ec;
   auto const n = resp3::read(stream, buf, adapter, ec);

   if (ec)
       BOOST_THROW_EXCEPTION(boost::system::system_error{ec});

   return n;
}

/** @brief Reads the response to a Redis command asynchronously.
 *  \ingroup functions
 *
 *  This function has to be called once for each command in the
 *  request until the whole request has been read.
 *
 *  The completion handler must have the following signature.
 *
 *  @code
 *  void(boost::system::error_code, std::size_t)
 *  @endcode
 *
 *  The second argumet to the completion handler is the number of
 *  bytes that have been consumed in the read operation.
 *
 *  \param stream The stream from which to read.
 *  \param buffer Auxiliary read buffer, usually a `std::string`.
 *  \param adapter The response adapter, see adapt.
 *  \param token The completion token.
 */
template <
   class AsyncReadStream,
   class DynamicBuffer,
   class ResponseAdapter = detail::response_traits<void>::adapter_type,
   class CompletionToken = net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read(
   AsyncReadStream& stream,
   DynamicBuffer buffer,
   ResponseAdapter adapter = adapt(),
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code, std::size_t)
      >(detail::parse_op<AsyncReadStream, DynamicBuffer, ResponseAdapter> {stream, buffer, adapter},
        token,
        stream);
}

/** \brief Reads the RESP3 type of the next incomming.
 *  \ingroup functions
 *
 *  This function won't consume any data from the buffer. The
 *  completion handler must have the following signature.
 *
 *  @code
    void(boost::system::error_code, type)
 *  @endcode
 *  
 *  \param stream The stream from which to read.
 *  \param buffer Auxiliary read buffer, usually a `std::string`.
 *  \param token The completion token.
 */
template <
   class AsyncReadStream,
   class DynamicBuffer,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read_type(
   AsyncReadStream& stream,
   DynamicBuffer buffer,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code, type)
      >(detail::type_op<AsyncReadStream, DynamicBuffer> {stream, buffer}, token, stream);
}

} // resp3
} // aedis

#include <boost/asio/unyield.hpp>
