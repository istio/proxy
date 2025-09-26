// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_TRIVIAL_LEGACY_TYPE_INFO_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_TRIVIAL_LEGACY_TYPE_INFO_H_

#include <string>

#include "absl/base/no_destructor.h"
#include "absl/strings/string_view.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/legacy_type_info_apis.h"

namespace google::api::expr::runtime {

// Implementation of type info APIs suitable for testing where no message
// operations need to be supported.
class TrivialTypeInfo : public LegacyTypeInfoApis {
 public:
  absl::string_view GetTypename(const MessageWrapper& wrapper) const override {
    return "opaque";
  }

  std::string DebugString(const MessageWrapper& wrapper) const override {
    return "opaque";
  }

  const LegacyTypeAccessApis* GetAccessApis(
      const MessageWrapper& wrapper) const override {
    // Accessors unsupported -- caller should treat this as an opaque type (no
    // fields defined, field access always results in a CEL error).
    return nullptr;
  }

  static const TrivialTypeInfo* GetInstance() {
    static absl::NoDestructor<TrivialTypeInfo> kInstance;
    return &*kInstance;
  }
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_TRIVIAL_LEGACY_TYPE_INFO_H_
