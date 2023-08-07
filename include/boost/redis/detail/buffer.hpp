#ifndef BOOST_REDIS_BUFFER_HPP
#define BOOST_REDIS_BUFFER_HPP

#include <limits>
#include <string>
#include <algorithm>

#include <boost/asio/buffer.hpp>
#include <boost/throw_exception.hpp>

namespace boost::redis::detail
{

template <class Elem, class Traits, class Allocator>
class dynamic_string_buffer {
public:
   using const_buffers_type = asio::const_buffer;
   using mutable_buffers_type = asio::mutable_buffer;

   explicit
   dynamic_string_buffer(
      std::basic_string<Elem, Traits, Allocator>& s,
      std::size_t maximum_size) noexcept
   : string_(s)
   , max_size_(maximum_size)
   {
   }

   void clear()
   {
      consumed_ = 0;
   }

   std::size_t size() const noexcept
   {
      return (std::min)(string_.size() - consumed_, max_size_);
   }

   bool empty() const noexcept
   {
      return size() == 0;
   }

   std::size_t max_size() const noexcept
   {
      return max_size_;
   }

   char const& front() const noexcept
   {
      return string_.at(consumed_);
   }

   std::size_t capacity() const noexcept
   {
      return (std::min)(string_.capacity(), max_size());
   }

   mutable_buffers_type data(std::size_t pos, std::size_t n) noexcept
   {
      return mutable_buffers_type(asio::buffer(asio::buffer(string_, max_size_) + pos + consumed_, n));
   }

   const_buffers_type data(std::size_t pos, std::size_t n) const noexcept
   {
      return const_buffers_type(asio::buffer(asio::buffer(string_, max_size_) + pos + consumed_, n));
   }

   void grow(std::size_t n)
   {
      if (string_.size() > max_size_ || (string_.size() + n) > max_size_)
         BOOST_THROW_EXCEPTION(std::length_error{"dynamic_string_buffer too long"});

      string_.resize(string_.size() + n);
   }

   void shrink(std::size_t n)
   {
      string_.resize(n > (string_.size() - consumed_) ? consumed_ : string_.size() - n);
   }

   void consume(std::size_t n, std::size_t tolerance = 100'000)
   {
      consumed_ += n;
      if (consumed_ > tolerance) {
         string_.erase(0, consumed_);
         consumed_ = 0;
      }
   }

private:
   std::basic_string<Elem, Traits, Allocator>& string_;
   std::size_t consumed_ = 0;
   std::size_t const max_size_;
};

}

#endif // BOOST_REDIS_BUFFER_HPP
