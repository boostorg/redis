/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string_view>
#include <aedis/net.hpp>
#include <aedis/resp3/response_adapter_base.hpp>

namespace aedis {
namespace resp3 {
namespace detail {

// resp3 parser.
class parser {
public:
   enum class bulk_type
   { blob_error
   , verbatim_string 
   , blob_string
   , streamed_string_part
   , none
   };

private:
   response_adapter_base* res_;
   int depth_;
   int sizes_[6]; // Streaming will require a bigger integer.
   bulk_type bulk_;
   int bulk_length_;

   void init(response_adapter_base* res);
   void on_aggregate(type t, char const* data);
   void on_null();
   void on_data(type t, char const* data, std::size_t n);
   void on_bulk(bulk_type b, std::string_view s = {});
   void on_blob(char const* data, bulk_type b);
   void on_blob_string(char const* data);

public:
   parser(response_adapter_base* res);
   std::size_t advance(char const* data, std::size_t n);
   auto done() const noexcept { return depth_ == 0 && bulk_ == bulk_type::none; }
   auto bulk() const noexcept { return bulk_; }
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
   parse_op(AsyncReadStream& stream, Storage* buf, response_adapter_base* res)
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
            if (parser_.bulk() == detail::parser::bulk_type::none) {
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
