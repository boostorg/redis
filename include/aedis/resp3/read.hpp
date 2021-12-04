/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/net.hpp>

#include <aedis/resp3/request.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/response.hpp>
#include <aedis/resp3/response_base.hpp>
#include <aedis/resp3/detail/parser.hpp>
#include <aedis/resp3/write.hpp>

#include <boost/asio/yield.hpp>

namespace aedis {
namespace resp3 {

/** \file read.hpp
    \brief Read utility functions.
  
    Synchronous and asynchronous to read responses to redis commands.
 */

template <class SyncReadStream, class Storage>
auto read(
   SyncReadStream& stream,
   Storage& buf,
   response_base& res,
   boost::system::error_code& ec)
{
   detail::parser p {&res};
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

/** \brief Redis a reponse to a command.
 *  
 *  \param stream Synchronous read stream from which the response will be read.
 *  \param buf Buffer for temporary storage e.g. std::string or std::vector<char>.
 *  \param res Reference to the response.
 *  \returns The number of bytes that have been read.
 */
template<class SyncReadStream, class Storage>
std::size_t
read(
   SyncReadStream& stream,
   Storage& buf,
   response_base& res)
{
   boost::system::error_code ec;
   auto const n = read(stream, buf, res, ec);

   if (ec)
       BOOST_THROW_EXCEPTION(boost::system::system_error{ec});

   return n;
}

/** @brief Reads the next command from a redis response
 *
 *  This function has to be called once for each command until the whole
 *  response has been cosumed.
 */
template <
   class AsyncReadStream,
   class Response,
   class Storage,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read(
   AsyncReadStream& stream,
   Storage& buffer,
   Response& resp,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code)
      >(detail::parse_op<AsyncReadStream, Storage> {stream, &buffer, &resp},
        token,
        stream);
}

// TODO: Move to detail.
type to_type(char c)
{
   switch (c) {
      case '!': return type::blob_error;
      case '=': return type::verbatim_string;
      case '$': return type::blob_string;
      case ';': return type::streamed_string_part;
      case '-': return type::simple_error;
      case ':': return type::number;
      case ',': return type::doublean;
      case '#': return type::boolean;
      case '(': return type::big_number;
      case '+': return type::simple_string;
      case '_': return type::null;
      case '>': return type::push;
      case '~': return type::set;
      case '*': return type::array;
      case '|': return type::attribute;
      case '%': return type::map;
      default: return type::invalid;
   }
}

// TODO: Move to detail.
template <class AsyncReadStream, class Storage>
class type_op {
private:
   AsyncReadStream& stream_;
   Storage* buf_ = nullptr;

public:
   type_op(AsyncReadStream& stream, Storage* buf)
   : stream_ {stream}
   , buf_ {buf}
   {
      assert(buf_);
   }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      if (ec) {
	 self.complete(ec, type::invalid);
         return;
      }

      if (std::empty(*buf_)) {
	 net::async_read_until(
	    stream_,
	    net::dynamic_buffer(*buf_),
	    "\r\n",
	    std::move(self));
	 return;
      }

      assert(!std::empty(*buf_));
      auto const type = to_type(buf_->front());
      self.complete(ec, type);
      return;
   }
};

/** \brief Asynchronously reads the type of the next incomming request.
 */
template <
   class AsyncReadStream,
   class Storage,
   class CompletionToken =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>
   >
auto async_read_type(
   AsyncReadStream& stream,
   Storage& buffer,
   CompletionToken&& token =
      net::default_completion_token_t<typename AsyncReadStream::executor_type>{})
{
   return net::async_compose
      < CompletionToken
      , void(boost::system::error_code, type)
      >(type_op<AsyncReadStream, Storage> {stream, &buffer}, token, stream);
}

} // resp3
} // aedis

#include <boost/asio/unyield.hpp>
