//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Nikolai Vladimirov (nvladimirov.work@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/resp3/flat_tree.hpp>
#include <boost/assert.hpp>

namespace boost::redis::resp3 {

flat_tree::flat_tree(flat_tree const& other)
: data_{other.data_}
, view_tree_{other.view_tree_}
, ranges_{other.ranges_}
, pos_{0u}
, reallocs_{0u}
, total_msgs_{other.total_msgs_}
{
   view_tree_.resize(ranges_.size());
   set_views();
}

flat_tree&
flat_tree::operator=(flat_tree other)
{
    swap(*this, other);
    return *this;
}

void flat_tree::reserve(std::size_t bytes, std::size_t nodes)
{
   data_.reserve(bytes);
   view_tree_.reserve(nodes);
   ranges_.reserve(nodes);
}

void flat_tree::clear()
{
   pos_ = 0u;
   total_msgs_ = 0u;
   reallocs_ = 0u;
   data_.clear();
   view_tree_.clear();
   ranges_.clear();
}

void flat_tree::set_views()
{
   BOOST_ASSERT_MSG(pos_ < view_tree_.size(), "notify_done called but no nodes added.");
   BOOST_ASSERT_MSG(view_tree_.size() == ranges_.size(), "Incompatible sizes.");

   for (; pos_ < view_tree_.size(); ++pos_) {
      auto const& r = ranges_.at(pos_);
      view_tree_.at(pos_).value = std::string_view{data_.data() + r.offset, r.size};
   }
}

void flat_tree::notify_done()
{
   total_msgs_ += 1;
   set_views();
}

void flat_tree::push(node_view const& node)
{
   auto data_before = data_.data();
   add_node_impl(node);
   auto data_after = data_.data();

   if (data_after != data_before) {
      pos_ = 0;
      reallocs_ += 1;
   }
}

void flat_tree::add_node_impl(node_view const& node)
{
   ranges_.push_back({data_.size(), node.value.size()});

   // This must come after setting the offset above.
   data_.insert(data_.end(), node.value.begin(), node.value.end());

   view_tree_.push_back(node);
}

void swap(flat_tree& a, flat_tree& b)
{
   using std::swap;

   swap(a.data_, b.data_);
   swap(a.view_tree_, b.view_tree_);
   swap(a.ranges_, b.ranges_);
   swap(a.pos_, b.pos_);
   swap(a.reallocs_, b.reallocs_);
   swap(a.total_msgs_, b.total_msgs_);
}

bool
operator==(
   flat_tree::range const& a,
   flat_tree::range const& b)
{
   return a.offset == b.offset && a.size == b.size;
}

bool operator==(flat_tree const& a, flat_tree const& b)
{
   return
      a.data_       == b.data_ &&
      a.view_tree_  == b.view_tree_ &&
      a.ranges_     == b.ranges_ &&
      a.pos_        == b.pos_ &&
      //a.reallocs_   == b.reallocs_ &&
      a.total_msgs_ == b.total_msgs_;
}

bool operator!=(flat_tree const& a, flat_tree const& b)
{
   return !(a == b);
}

}  // namespace boost::redis
