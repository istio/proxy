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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_CONSTANT_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_CONSTANT_H_

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "common/constant.h"

namespace cel::extensions::protobuf_internal {

// `ConstantToProto` converts from native `Constant` to its protocol buffer
// message equivalent.
absl::Status ConstantToProto(const Constant& constant,
                             absl::Nonnull<google::api::expr::v1alpha1::Constant*> proto);

// `ConstantToProto` converts to native `Constant` from its protocol buffer
// message equivalent.
absl::Status ConstantFromProto(const google::api::expr::v1alpha1::Constant& proto,
                               Constant& constant);

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_CONSTANT_H_
