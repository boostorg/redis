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
 *  Users are allowed/encouraged to override this class to customize responses.
 */
struct response_base {

   /** @brief Function called by the parser when new data has been processed.
    *  
    *  Users who what to customize their response types are required to derive
    *  from this class and override this function, see examples.
    *
    *  \param t The RESP3 type of the data.
    *
    *  \param n When t is an aggregate data type this will contain its size
    *     (see also element_multiplicity) for simple data types this is always 1.
    *
    *  \param depth The element depth in the tree.
    *
    *  \param data A pointer to the data.
    *
    *  \param size The size of data.
    */
   virtual void
   add(
      type t,
      std::size_t n,
      std::size_t depth,
      char const* data = nullptr,
      std::size_t size = 0)
   {}

   /** @brief Virtual destructor to allow inheritance.
    */
   virtual ~response_base() {}
};

} // resp3
} // aedis
