// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_ANY_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ANY_H_

#include <string>

#include "google/protobuf/any.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"

namespace cel {

inline google::protobuf::Any MakeAny(absl::string_view type_url,
                                     const absl::Cord& value) {
  google::protobuf::Any any;
  any.set_type_url(type_url);
  any.set_value(static_cast<std::string>(value));
  return any;
}

inline google::protobuf::Any MakeAny(absl::string_view type_url,
                                     absl::string_view value) {
  google::protobuf::Any any;
  any.set_type_url(type_url);
  any.set_value(value);
  return any;
}

inline absl::Cord GetAnyValueAsCord(const google::protobuf::Any& any) {
  return absl::Cord(any.value());
}

inline std::string GetAnyValueAsString(const google::protobuf::Any& any) {
  return std::string(any.value());
}

inline void SetAnyValueFromCord(absl::Nonnull<google::protobuf::Any*> any,
                                const absl::Cord& value) {
  any->set_value(static_cast<std::string>(value));
}

inline absl::string_view GetAnyValueAsStringView(
    const google::protobuf::Any& any ABSL_ATTRIBUTE_LIFETIME_BOUND,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return absl::string_view(any.value());
}

inline constexpr absl::string_view kTypeGoogleApisComPrefix =
    "type.googleapis.com/";

inline std::string MakeTypeUrlWithPrefix(absl::string_view prefix,
                                         absl::string_view type_name) {
  return absl::StrCat(absl::StripSuffix(prefix, "/"), "/", type_name);
}

inline std::string MakeTypeUrl(absl::string_view type_name) {
  return MakeTypeUrlWithPrefix(kTypeGoogleApisComPrefix, type_name);
}

bool ParseTypeUrl(absl::string_view type_url,
                  absl::Nullable<absl::string_view*> prefix,
                  absl::Nullable<absl::string_view*> type_name);
inline bool ParseTypeUrl(absl::string_view type_url,
                         absl::Nullable<absl::string_view*> type_name) {
  return ParseTypeUrl(type_url, nullptr, type_name);
}
inline bool ParseTypeUrl(absl::string_view type_url) {
  return ParseTypeUrl(type_url, nullptr);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ANY_H_
