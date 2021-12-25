/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/net.hpp>

#include <aedis/resp3/serializer.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/write.hpp>
#include <aedis/resp3/response_traits.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/detail/read_ops.hpp>

#include <boost/asio/yield.hpp>

namespace aedis {
namespace resp3 {

/** \file read.hpp
    \brief Read utility functions.
  
    Synchronous and asynchronous utility functions.
 */

template <
  class SyncReadStream,
  class Buffer,
  class ResponseAdapter
  >
auto read(
   SyncReadStream& stream,
   Buffer& buf,
   ResponseAdapter adapter,
   boost::system::error_code& ec)
{
   detail::parser p {adapter};
   std::size_t n = 0;
   do {
      if (p.bulk() == type::invalid) {
	 n = net::read_until(stream, net::dynamic_buffer(buf), "\r\n", ec);
	 if (ec || n < 3)
	    return n;
      } else {
	 auto const s = std::ssize(buf);
	 auto const l = p.bulk_length();
	 if (s < (l + 2)) {
	    buf.resize(l + 2);
	    auto const to_read = static_cast<std::size_t>(l + 2 - s);
	    n = net::read(stream, net::buffer(buf.data() + s, to_read));
	    assert(n >= to_read);
	    if (ec)
	       return n;
	 }
      }

      n = p.advance(buf.data(), n);
      buf.erase(0, n);
   } while (!p.done());

   return n;
}

/** \brief Reads the reponse to a command.
 *  
 *  \param stream Synchronous read stream from which the response will be read.
 *  \param buf Buffer for temporary storage e.g. std::string or std::vector<char>.
 *  \param adapter Reference to the response.
 *  \returns The number of bytes that have been read.
 */
template<
   class SyncReadStream,
   class Buffer,
   class ResponseAdapter>
std::size_t
read(
   SyncReadStream& stream,
   Buffer& buf,
   ResponseAdapter adapter)
{
   boost::system::error_code ec;
   auto const n = read(stream, buf, adapter, ec);

   if (ec)
       BOOST_THROW_EXCEPTION(boost::system::system_error{ec});

   return n;
}

/** @brief Reads the response to a Redis command.
  
    This function has to be called once for each command in the
    request until the whole request has been consumed.
 */
template <
   class AsyncReadStream,
   class Buffer,
   class ResponseAdapter = response_traits<void>::adapter_type,
   class CompletionToken = net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read(
   AsyncReadStream& stream,
   Buffer& buffer,
   ResponseAdapter adapter = adapt(),
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code, std::size_t)
      >(detail::parse_op<AsyncReadStream, Buffer, ResponseAdapter> {stream, &buffer, adapter},
        token,
        stream);
}

/** \brief Asynchronously reads the type of the next incomming request.
 */
template <
   class AsyncReadStream,
   class Buffer,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read_type(
   AsyncReadStream& stream,
   Buffer& buffer,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code, type)
      >(detail::type_op<AsyncReadStream, Buffer> {stream, &buffer}, token, stream);
}

} // resp3
} // aedis

#include <boost/asio/unyield.hpp>
