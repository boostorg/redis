//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Nikolai Vladimirov (nvladimirov.work@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/error.hpp>
#include <boost/redis/generic_flat_response_value.hpp>

#include <boost/assert.hpp>

namespace boost::redis {

void generic_flat_response_value::reserve(std::size_t bytes, std::size_t nodes)
{
   data_.reserve(bytes);
   view_.reserve(nodes);
}

void generic_flat_response_value::clear()
{
   pos_ = 0u;
   total_msgs_ = 0u;
   reallocs_ = 0u;
   data_.clear();
   view_.clear();
}

void generic_flat_response_value::notify_done()
{
   BOOST_ASSERT_MSG(pos_ < view_.size(), "notify_done called but no nodes added.");

   total_msgs_ += 1;

   for (; pos_ < view_.size(); ++pos_) {
      auto& v = view_.at(pos_).value;
      v.data = std::string_view{data_.data() + v.offset, v.size};
   }
}

void generic_flat_response_value::push(resp3::node_view const& nd)
{
   auto data_before = data_.data();
   add_node_impl(nd);
   auto data_after = data_.data();

   if (data_after != data_before) {
      pos_ = 0;
      reallocs_ += 1;
   }
}

void generic_flat_response_value::add_node_impl(resp3::node_view const& nd)
{
   resp3::offset_node ond;
   ond.data_type = nd.data_type;
   ond.aggregate_size = nd.aggregate_size;
   ond.depth = nd.depth;
   ond.value.offset = data_.size();
   ond.value.size = nd.value.size();

   // This must come after setting the offset above.
   data_.append(nd.value.data(), nd.value.size());

   view_.push_back(std::move(ond));
}
}  // namespace boost::redis
