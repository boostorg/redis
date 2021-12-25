/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string_view>
#include <aedis/resp3/detail/parser.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

// The parser supports up to 5 levels of nested structures. The first
// element in the sizes stack is a sentinel and must be different from
// 1.
template <
   class AsyncReadStream,
   class Buffer,
   class ResponseAdapter>
class parse_op {
private:
   AsyncReadStream& stream_;
   Buffer* buf_ = nullptr;
   parser<ResponseAdapter> parser_;
   int start_ = 1;

public:
   parse_op(AsyncReadStream& stream, Buffer* buf, ResponseAdapter adapter)
   : stream_ {stream}
   , buf_ {buf}
   , parser_ {adapter}
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
               net::async_read_until(
                  stream_,
                  net::dynamic_buffer(*buf_),
                  "\r\n",
                  std::move(self));

               return;
            }

	    // On a bulk read we can't read until delimiter since the
	    // payload may contain the delimiter itself so we have to
	    // read the whole chunk. However if the bulk blob is small
	    // enough it may be already on the buffer buf_ we read
	    // last time. If it is, there is no need of initiating
	    // another async op otherwise we have to read the
	    // missing bytes.
            if (std::ssize(*buf_) < (parser_.bulk_length() + 2)) {
               start_ = 0;
	       auto const s = std::ssize(*buf_);
	       auto const l = parser_.bulk_length();
	       auto const to_read = static_cast<std::size_t>(l + 2 - s);
               buf_->resize(l + 2);
               net::async_read(
                  stream_,
                  net::buffer(buf_->data() + s, to_read),
                  net::transfer_all(),
                  std::move(self));
               return;
            }

            default:
	    {
	       if (ec)
		  return self.complete(ec);

	       n = parser_.advance(buf_->data(), n);
	       buf_->erase(0, n);
	       if (parser_.done())
		  return self.complete({});
	    }
         }
      }
   }
};

template <class AsyncReadStream, class Buffer>
class type_op {
private:
   AsyncReadStream& stream_;
   Buffer* buf_ = nullptr;

public:
   type_op(AsyncReadStream& stream, Buffer* buf)
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

} // detail
} // resp3
} // aedis
