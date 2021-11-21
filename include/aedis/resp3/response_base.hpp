/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/type.hpp>

#include <string_view>

namespace aedis {
namespace resp3 {

/** @brief A base class for all response types.
 *
 *  Users are allowed to override this class to customize responses.
 */
struct response_base {
   /** @brief Function called by the parser when new data has been processed.
    *  
    *  Users who what to customize their response types are required to derive
    *  from this class and override this function, see examples.
    */
   virtual
   void add(type t, std::size_t n, std::size_t depth, char const* data = nullptr, std::size_t size = 0) {}

   /** @brief Virtual destructor to allow inheritance.
    */
   virtual ~response_base() {}
};

} // resp3
} // aedis
