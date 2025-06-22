//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/impl/log_to_file.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cstdio>
#include <memory>
#include <string>

using namespace boost::redis;

namespace {

// RAII helpers for working with C FILE*
struct file_deleter {
   void operator()(FILE* f) const { std::fclose(f); }
};
using unique_file = std::unique_ptr<FILE, file_deleter>;

std::string get_file_contents(FILE* f)
{
   std::fseek(f, 0, SEEK_END);
   long fsize = std::ftell(f);
   if (!BOOST_TEST_GE(fsize, 0))
      exit(1);
   std::rewind(f);
   std::string res(fsize, 0);
   std::fread(res.data(), res.size(), 1u, f);
   return res;
}

void test_regular()
{
   unique_file f{std::tmpfile()};
   if (!BOOST_TEST_NE(f.get(), nullptr))
      exit(1);

   detail::log_to_file(f.get(), "something happened");

   auto contents = get_file_contents(f.get());
   BOOST_TEST_EQ(contents, "(Boost.Redis) something happened\n");
}

}  // namespace

int main()
{
   test_regular();

   return boost::report_errors();
}
