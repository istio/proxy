// Copyright 2024 Google LLC
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

#include <string>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/strings/str_cat.h"
#include "common/type.h"
#include "google/protobuf/descriptor.h"

namespace cel {

using google::protobuf::EnumDescriptor;

bool IsWellKnownEnumType(absl::Nonnull<const EnumDescriptor*> descriptor) {
  return descriptor->full_name() == "google.protobuf.NullValue";
}

std::string EnumType::DebugString() const {
  if (ABSL_PREDICT_TRUE(static_cast<bool>(*this))) {
    static_assert(sizeof(descriptor_) == 8 || sizeof(descriptor_) == 4,
                  "sizeof(void*) is neither 8 nor 4");
    return absl::StrCat(name(), "@0x",
                        absl::Hex(descriptor_, sizeof(descriptor_) == 8
                                                   ? absl::PadSpec::kZeroPad16
                                                   : absl::PadSpec::kZeroPad8));
  }
  return std::string();
}

}  // namespace cel
