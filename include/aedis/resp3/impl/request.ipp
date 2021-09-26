/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <aedis/command.hpp>

namespace aedis { namespace resp3 {

void add_bulk(std::string& to, std::string_view param)
{
   to += "$";
   to += std::to_string(std::size(param));
   to += "\r\n";
   to += param;
   to += "\r\n";
}

void add_header(std::string& to, int size)
{
   to += "*";
   to += std::to_string(size);
   to += "\r\n";
}

void assemble(std::string& ret, std::string_view cmd)
{
   add_header(ret, 1);
   add_bulk(ret, cmd);
}

void assemble(std::string& ret, std::string_view cmd, std::string_view key)
{
   std::initializer_list<std::string_view> dummy;
   assemble(ret, cmd, {key}, std::cbegin(dummy), std::cend(dummy));
}

} // resp3
} // aedis
