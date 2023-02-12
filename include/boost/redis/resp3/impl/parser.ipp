/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/resp3/parser.hpp>
#include <boost/redis/error.hpp>
#include <boost/assert.hpp>

#include <charconv>

namespace boost::redis::resp3 {

void to_int(int_type& i, std::string_view sv, system::error_code& ec)
{
   auto const res = std::from_chars(sv.data(), sv.data() + std::size(sv), i);
   if (res.ec != std::errc())
      ec = error::not_a_number;
}

parser::parser()
{
   sizes_[0] = 2; // The sentinel must be more than 1.
}

auto
parser::consume(
   char const* data,
   std::size_t n,
   system::error_code& ec) -> std::pair<node_type, std::size_t>
{
   node_type ret;
   if (bulk_expected()) {
      n = bulk_length_ + 2;
      ret = {bulk_, 1, depth_, {data, bulk_length_}};
      bulk_ = type::invalid;
      --sizes_[depth_];

   } else if (sizes_[depth_] != 0) {
      auto const t = to_type(*data);
      switch (t) {
         case type::streamed_string_part:
         {
            to_int(bulk_length_ , std::string_view{data + 1, n - 3}, ec);
            if (ec)
               return std::make_pair(node_type{}, 0);

            if (bulk_length_ == 0) {
               ret = {type::streamed_string_part, 1, depth_, {}};
               sizes_[depth_] = 0; // We are done.
               bulk_ = type::invalid;
            } else {
               bulk_ = type::streamed_string_part;
            }
         } break;
         case type::blob_error:
         case type::verbatim_string:
         case type::blob_string:
         {
            if (data[1] == '?') {
               // NOTE: This can only be triggered with blob_string.
               // Trick: A streamed string is read as an aggregate
               // of infinite lenght. When the streaming is done
               // the server is supposed to send a part with length
               // 0.
               sizes_[++depth_] = (std::numeric_limits<std::size_t>::max)();
               ret = {type::streamed_string, 0, depth_, {}};
            } else {
               to_int(bulk_length_ , std::string_view{data + 1, n - 3} , ec);
               if (ec)
                  return std::make_pair(node_type{}, 0);

               bulk_ = t;
            }
         } break;
         case type::boolean:
         {
            if (n == 3) {
                ec = error::empty_field;
                return std::make_pair(node_type{}, 0);
            }

            if (data[1] != 'f' && data[1] != 't') {
                ec = error::unexpected_bool_value;
                return std::make_pair(node_type{}, 0);
            }

            ret = {t, 1, depth_, {data + 1, n - 3}};
            --sizes_[depth_];
         } break;
         case type::doublean:
         case type::big_number:
         case type::number:
         {
            if (n == 3) {
                ec = error::empty_field;
                return std::make_pair(node_type{}, 0);
            }

            ret = {t, 1, depth_, {data + 1, n - 3}};
            --sizes_[depth_];
         } break;
         case type::simple_error:
         case type::simple_string:
         {
            ret = {t, 1, depth_, {&data[1], n - 3}};
            --sizes_[depth_];
         } break;
         case type::null:
         {
            ret = {type::null, 1, depth_, {}};
            --sizes_[depth_];
         } break;
         case type::push:
         case type::set:
         case type::array:
         case type::attribute:
         case type::map:
         {
            int_type l = -1;
            to_int(l, std::string_view{data + 1, n - 3}, ec);
            if (ec)
               return std::make_pair(node_type{}, 0);

            ret = {t, l, depth_, {}};
            if (l == 0) {
               --sizes_[depth_];
            } else {
               if (depth_ == max_embedded_depth) {
                  ec = error::exceeeds_max_nested_depth;
                  return std::make_pair(node_type{}, 0);
               }

               ++depth_;

               sizes_[depth_] = l * element_multiplicity(t);
            }
         } break;
         default:
         {
            ec = error::invalid_data_type;
            return std::make_pair(node_type{}, 0);
         }
      }
   }
   
   while (sizes_[depth_] == 0) {
      --depth_;
      --sizes_[depth_];
   }
   
   return std::make_pair(ret, n);
}
} // boost::redis::resp3
