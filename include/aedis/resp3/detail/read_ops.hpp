/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string_view>
#include <aedis/net.hpp>
#include <aedis/resp3/detail/parser.hpp>

#include <boost/core/ignore_unused.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

// TODO: Use asio::coroutine.
template <
   class AsyncReadStream,
   class DynamicBuffer,
   class ResponseAdapter>
class parse_op {
private:
   AsyncReadStream& stream_;
   DynamicBuffer buf_;
   parser<ResponseAdapter> parser_;
   std::size_t consumed_;
   int start_;

public:
   parse_op(AsyncReadStream& stream, DynamicBuffer buf, ResponseAdapter adapter)
   : stream_ {stream}
   , buf_ {buf}
   , parser_ {adapter}
   , consumed_{0}
   , start_{1}
   { }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      switch (start_) {
         for (;;) {
            if (parser_.bulk() == type::invalid) {
               case 1:
               start_ = 0;
               net::async_read_until(stream_, buf_, "\r\n", std::move(self));
               return;
            }

	    // On a bulk read we can't read until delimiter since the
	    // payload may contain the delimiter itself so we have to
	    // read the whole chunk. However if the bulk blob is small
	    // enough it may be already on the buffer (from the last
	    // read), in which case there is no need of initiating
	    // another async op, otherwise we have to read the missing
	    // bytes.
            if (std::size(buf_) < (parser_.bulk_length() + 2)) {
               start_ = 0;
	       auto const s = std::size(buf_);
	       auto const l = parser_.bulk_length();
	       auto const to_read = l + 2 - s;
               buf_.grow(to_read);
               net::async_read(stream_, buf_.data(s, to_read), net::transfer_all(), std::move(self));
               return;
            }

            default:
	    {
	       if (ec) {
		  self.complete(ec, 0);
		  return;
	       }

	       n = parser_.advance((char const*)buf_.data(0, n).data(), n, ec);
	       if (ec) {
		  self.complete(ec, 0);
		  return;
	       }

	       buf_.consume(n);
	       consumed_ += n;
	       if (parser_.done()) {
		  self.complete({}, consumed_);
		  return;
	       }
	    }
         }
      }
   }
};

// TODO: Use asio::coroutine.
template <class AsyncReadStream, class DynamicBuffer>
class type_op {
private:
   AsyncReadStream& stream_;
   DynamicBuffer buf_;

public:
   type_op(AsyncReadStream& stream, DynamicBuffer buf)
   : stream_ {stream}
   , buf_ {buf}
   { }

   template <class Self>
   void operator()( Self& self
                  , boost::system::error_code ec = {}
                  , std::size_t n = 0)
   {
      boost::ignore_unused(n);

      if (ec) {
	 self.complete(ec, type::invalid);
         return;
      }

      if (std::size(buf_) == 0) {
	 net::async_read_until(stream_, buf_, "\r\n", std::move(self));
	 return;
      }

      auto const* data = (char const*)buf_.data(0, n).data();
      auto const type = to_type(*data);
      self.complete(ec, type);
      return;
   }
};

} // detail
} // resp3
} // aedis
