/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string_view>
#include <aedis/net.hpp>
#include <aedis/resp3/response_base.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

// resp3 parser.
class parser {
private:
   response_base* res_;
   std::size_t depth_;
   std::size_t sizes_[6];
   std::size_t bulk_length_;
   type bulk_;

   void init(response_base* res);

public:
   parser(response_base* res);

   // Returns the number of bytes in data that have been consumed.
   std::size_t advance(char const* data, std::size_t n);

   // returns true when the parser is done with the current message.
   auto done() const noexcept
      { return depth_ == 0 && bulk_ == type::invalid; }

   // The bulk type expected in the next read. If none is expected returns
   // type::invalid.
   auto bulk() const noexcept { return bulk_; }

   // The lenght of the next expected bulk_length.
   auto bulk_length() const noexcept { return bulk_length_; }
};

// The parser supports up to 5 levels of nested structures. The first
// element in the sizes stack is a sentinel and must be different from
// 1.
template <class AsyncReadStream, class Storage>
class parse_op {
private:
   AsyncReadStream& stream_;
   Storage* buf_ = nullptr;
   detail::parser parser_;
   int start_ = 1;

public:
   parse_op(AsyncReadStream& stream, Storage* buf, response_base* res)
   : stream_ {stream}
   , buf_ {buf}
   , parser_ {res}
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

} // detail
} // resp3
} // aedis
