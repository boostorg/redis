/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string_view>
#include <charconv>
#include <system_error>
#include <limits>

namespace aedis {
namespace resp3 {
namespace detail {

// Converts a wire-format char to a resp3 type.
type to_type(char c);

template <class ResponseAdapter>
class parser {
private:
   ResponseAdapter adapter_;
   std::size_t depth_;
   std::size_t sizes_[6];
   std::size_t bulk_length_;
   type bulk_;

   void init()
   {
      depth_ = 0;
      sizes_[0] = 2;
      sizes_[1] = 1;
      sizes_[2] = 1;
      sizes_[3] = 1;
      sizes_[4] = 1;
      sizes_[5] = 1;
      sizes_[6] = 1;
      bulk_ = type::invalid;
      bulk_length_ = (std::numeric_limits<std::size_t>::max)();
   }

public:
   parser(ResponseAdapter adapter)
   : adapter_{adapter}
   { init(); }

   // Returns the number of bytes that have been consumed.
   std::size_t
   advance(char const* data, std::size_t n, std::error_code& ec)
   {
      if (bulk_ != type::invalid) {
         n = bulk_length_ + 2;
         switch (bulk_) {
            case type::streamed_string_part:
            {
              if (bulk_length_ == 0) {
                 sizes_[depth_] = 1;
              } else {
                 adapter_(bulk_, 1, depth_, data, bulk_length_);
              }
            } break;
            default: adapter_(bulk_, 1, depth_, data, bulk_length_);
         }

         bulk_ = type::invalid;
         --sizes_[depth_];

      } else if (sizes_[depth_] != 0) {
         auto const t = to_type(*data);
         switch (t) {
            case type::blob_error:
            case type::verbatim_string:
            case type::streamed_string_part:
            {
               auto const r =
		  std::from_chars(data + 1, data + n - 2, bulk_length_);
	       if (r.ec != std::errc()) {
		  ec = std::make_error_code(r.ec);
		  return 0;
	       }

               bulk_ = t;
            } break;
            case type::blob_string:
            {
               if (*(data + 1) == '?') {
                  sizes_[++depth_] = (std::numeric_limits<std::size_t>::max)();
               } else {
		  auto const r =
		     std::from_chars(data + 1, data + n - 2, bulk_length_);
		  if (r.ec != std::errc()) {
		     ec = std::make_error_code(r.ec);
		     return 0;
		  }

                  bulk_ = type::blob_string;
               }
            } break;
            case type::simple_error:
            case type::number:
            case type::doublean:
            case type::boolean:
            case type::big_number:
            case type::simple_string:
            {
               adapter_(t, 1, depth_, data + 1, n - 3);
               --sizes_[depth_];
            } break;
            case type::null:
            {
               adapter_(type::null, 1, depth_, nullptr, 0);
               --sizes_[depth_];
            } break;
            case type::push:
            case type::set:
            case type::array:
            case type::attribute:
            case type::map:
            {
	       std::size_t l;
               auto const r = std::from_chars(data + 1, data + n - 2, l);
	       if (r.ec != std::errc()) {
		  ec = std::make_error_code(r.ec);
		  return 0;
	       }

               adapter_(t, l, depth_, nullptr, 0);

               if (l == 0) {
                  --sizes_[depth_];
               } else {
                  auto const m = element_multiplicity(t);
                  sizes_[++depth_] = m * l;
               }
            } break;
            default:
            {
               // TODO: This should cause an error not an assert.
               assert(false);
            }
         }
      }
      
      while (sizes_[depth_] == 0)
         --sizes_[--depth_];
      
      return n;
   }


   // returns true when the parser is done with the current message.
   auto done() const noexcept
      { return depth_ == 0 && bulk_ == type::invalid; }

   // The bulk type expected in the next read. If none is expected returns
   // type::invalid.
   auto bulk() const noexcept { return bulk_; }

   // The lenght of the next expected bulk_length.
   auto bulk_length() const noexcept { return bulk_length_; }
};

} // detail
} // resp3
} // aedis
