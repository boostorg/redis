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
   view_resp_.reserve(nodes);
   ranges_.reserve(nodes);
}

void generic_flat_response_value::clear()
{
   pos_ = 0u;
   total_msgs_ = 0u;
   reallocs_ = 0u;
   data_.clear();
   view_resp_.clear();
   ranges_.clear();
}

void generic_flat_response_value::notify_done()
{
   BOOST_ASSERT_MSG(pos_ < view_resp_.size(), "notify_done called but no nodes added.");

   BOOST_ASSERT_MSG(view_resp_.size() == ranges_.size(), "Incompatible sizes.");

   total_msgs_ += 1;

   for (; pos_ < view_resp_.size(); ++pos_) {
      auto const& r = ranges_.at(pos_);
      view_resp_.at(pos_).value = std::string_view{data_.data() + r.offset, r.size};
   }
}

void generic_flat_response_value::push(resp3::node_view const& node)
{
   auto data_before = data_.data();
   add_node_impl(node);
   auto data_after = data_.data();

   if (data_after != data_before) {
      pos_ = 0;
      reallocs_ += 1;
   }
}

void generic_flat_response_value::add_node_impl(resp3::node_view const& node)
{
   ranges_.push_back({data_.size(), node.value.size()});

   // This must come after setting the offset above.
   data_.append(node.value.data(), node.value.size());

   view_resp_.push_back(node);
}
}  // namespace boost::redis
