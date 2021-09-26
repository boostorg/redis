/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/adapter_utils.hpp>
#include <aedis/resp3/ignore_adapter.hpp>
#include <aedis/resp3/array_adapter.hpp>
#include <aedis/resp3/flat_map_adapter.hpp>
#include <aedis/resp3/flat_set_adapter.hpp>
#include <aedis/resp3/basic_flat_array_adapter.hpp>
#include <aedis/resp3/number_adapter.hpp>
#include <aedis/resp3/blob_string_adapter.hpp>
#include <aedis/resp3/simple_string_adapter.hpp>
#include <aedis/resp3/blob_error_adapter.hpp>
#include <aedis/resp3/simple_error_adapter.hpp>
#include <aedis/resp3/big_number_adapter.hpp>
#include <aedis/resp3/doublean_adapter.hpp>
#include <aedis/resp3/verbatim_string_adapter.hpp>
#include <aedis/resp3/boolean_adapter.hpp>
#include <aedis/resp3/streamed_string_part_adapter.hpp>

namespace aedis { namespace resp3 {

class response {
private:
   array_type array_;
   array_adapter array_adapter_{&array_};

   flat_array_type flat_array_;
   basic_flat_array_adapter<std::string> flat_array_adapter_{&flat_array_};

   flat_array_type flat_attribute_;
   basic_flat_array_adapter<std::string> flat_attribute_adapter_{&flat_attribute_};

   flat_array_type flat_push_;
   basic_flat_array_adapter<std::string> flat_push_adapter_{&flat_push_};

   big_number_type big_number_;
   big_number_adapter big_number_adapter_{&big_number_};

   blob_error_type blob_error_;
   blob_error_adapter blob_error_adapter_{&blob_error_};

   blob_string_type blob_string_;
   blob_string_adapter blob_string_adapter_{&blob_string_};

   boolean_type boolean_;
   boolean_adapter boolean_adapter_{&boolean_};

   doublean_type doublean_;
   doublean_adapter doublean_adapter_{&doublean_};

   flat_map_type flat_map_;
   flat_map_adapter flat_map_adapter_{&flat_map_};

   flat_set_type flat_set_;
   flat_set_adapter flat_set_adapter_{&flat_set_};

   number_type number_;
   number_adapter number_adapter_{&number_};

   simple_error_type simple_error_;
   simple_error_adapter simple_error_adapter_{&simple_error_};

   simple_string_type simple_string_;
   simple_string_adapter simple_string_adapter_{&simple_string_};

   streamed_string_part_type streamed_string_part_;
   streamed_string_part_adapter streamed_string_part_adapter_{&streamed_string_part_};

   verbatim_string_type verbatim_string_;
   verbatim_string_adapter verbatim_string_adapter_{&verbatim_string_};

   ignore_adapter ignore_adapter_;

public:
   response_adapter_base* select_adapter(resp3::type type, command cmd);

   auto const& array() const noexcept {return array_;}
   auto& array() noexcept {return array_;}

   auto const& flat_push() const noexcept {return flat_push_;}
   auto& flat_push() noexcept {return flat_push_;}

   auto const& flat_array() const noexcept {return flat_array_;}
   auto& flat_array() noexcept {return flat_array_;}

   auto const& flat_map() const noexcept {return flat_map_;}
   auto& flat_map() noexcept {return flat_map_;}

   auto const& flat_set() const noexcept {return flat_set_;}
   auto& flat_set() noexcept {return flat_set_;}

   auto const& simple_string() const noexcept {return simple_string_;}
   auto& simple_string() noexcept {return simple_string_;}

   auto const& number() const noexcept {return number_;}
   auto& number() noexcept {return number_;}

   auto const& boolean() const noexcept {return boolean_;}
   auto& boolean() noexcept {return boolean_;}

   auto const& blob_string() const noexcept {return blob_string_;}
   auto& blob_string() noexcept {return blob_string_;}

   auto const& blob_error() const noexcept {return blob_error_;}
   auto& blob_error() noexcept {return blob_error_;}

   auto const& streamed_string_part() const noexcept {return streamed_string_part_;}
   auto& streamed_string_part() noexcept {return streamed_string_part_;}
};

} // resp3
} // aedis
