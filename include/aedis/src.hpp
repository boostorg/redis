/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/** \file src.hpp
 *  \brief A list of source files that must built in order to use aedis.
 *
 *  Include this file in no more than one source file in your application.
 */

#include <aedis/impl/command.ipp>
#include <aedis/resp3/impl/type.ipp>
#include <aedis/resp3/impl/node.ipp>
#include <aedis/resp3/detail/impl/composer.ipp>
#include <aedis/resp3/detail/impl/parser.ipp>
