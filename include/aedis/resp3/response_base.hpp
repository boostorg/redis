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

/** @brief Response adapter base class.
 *
 *  Users are allowed to override this class to customize responses.
 */
struct response_base {
   virtual void add(type t, int n, int depth, std::string_view s = {}) {}

   /** Virtual destructor to allow inheritance.
    */
   virtual ~response_base() {}
};

} // resp3
} // aedis
