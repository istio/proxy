/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <set>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"

namespace Wasm {
namespace Common {

#define STD_ISTIO_DIMENSIONS(FIELD_FUNC)                                                           \
  FIELD_FUNC(downstream_ip)                                                                        \
  FIELD_FUNC(reporter)                                                                             \
  FIELD_FUNC(source_workload)                                                                      \
  FIELD_FUNC(source_workload_namespace)                                                            \
  FIELD_FUNC(source_principal)                                                                     \
  FIELD_FUNC(source_app)                                                                           \
  FIELD_FUNC(source_version)                                                                       \
  FIELD_FUNC(source_canonical_service)                                                             \
  FIELD_FUNC(source_canonical_revision)                                                            \
  FIELD_FUNC(destination_workload)                                                                 \
  FIELD_FUNC(destination_workload_namespace)                                                       \
  FIELD_FUNC(destination_principal)                                                                \
  FIELD_FUNC(destination_app)                                                                      \
  FIELD_FUNC(destination_version)                                                                  \
  FIELD_FUNC(destination_service)                                                                  \
  FIELD_FUNC(destination_service_name)                                                             \
  FIELD_FUNC(destination_service_namespace)                                                        \
  FIELD_FUNC(destination_canonical_service)                                                        \
  FIELD_FUNC(destination_canonical_revision)                                                       \
  FIELD_FUNC(destination_port)                                                                     \
  FIELD_FUNC(request_protocol)                                                                     \
  FIELD_FUNC(response_code)                                                                        \
  FIELD_FUNC(grpc_response_status)                                                                 \
  FIELD_FUNC(response_flags)                                                                       \
  FIELD_FUNC(connection_security_policy)

// A structure that can hold multiple Istio dimensions(metadata variables).
// This could be use to key caches based on Istio dimensions for various
// filters.
// Note: This is supposed to be used with absl::flat_hash_map only.
// TODO: Add support for evaluating dynamic Istio dimensions.
struct IstioDimensions {
#define DEFINE_FIELD(name) std::string(name);
  STD_ISTIO_DIMENSIONS(DEFINE_FIELD)
#undef DEFINE_FIELD

  bool outbound = false;

#define SET_FIELD(name)                                                                            \
  IstioDimensions& set_##name(std::string value) {                                                 \
    name = value;                                                                                  \
    return *this;                                                                                  \
  }

  STD_ISTIO_DIMENSIONS(SET_FIELD)
#undef SET_FIELD

  IstioDimensions& set_outbound(bool value) {
    outbound = value;
    return *this;
  }

  std::string to_string() const {
#define TO_STRING(name) "\"", #name, "\":\"", name, "\" ,",
    return absl::StrCat("{" STD_ISTIO_DIMENSIONS(TO_STRING) "\"outbound\": ", outbound, "}");
#undef TO_STRING
  }

  // This function is required to make IstioDimensions type hashable.
  template <typename H> friend H AbslHashValue(H h, IstioDimensions d) {
#define TO_HASH_VALUE(name) , d.name
    return H::combine(std::move(h) STD_ISTIO_DIMENSIONS(TO_HASH_VALUE), d.outbound);
#undef TO_HASH_VALUE
  }

  // This function is required to make IstioDimensions type hashable.
  friend bool operator==(const IstioDimensions& lhs, const IstioDimensions& rhs) {
    return (
#define COMPARE(name) lhs.name == rhs.name&&
        STD_ISTIO_DIMENSIONS(COMPARE) lhs.outbound == rhs.outbound);
#undef COMPARE
  }
};

} // namespace Common
} // namespace Wasm
