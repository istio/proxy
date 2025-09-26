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
//
// Factories and constants for well-known CEL errors.
#ifndef THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_ERRORS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_ERRORS_H_

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "runtime/internal/errors.h"  // IWYU pragma: export
#include "google/protobuf/arena.h"

namespace cel {
namespace interop_internal {
// Factories for interop error values.
// const pointer Results are arena allocated to support interop with cel::Handle
// and expr::runtime::CelValue.
const absl::Status* CreateNoMatchingOverloadError(google::protobuf::Arena* arena,
                                                  absl::string_view fn);

const absl::Status* CreateNoSuchFieldError(google::protobuf::Arena* arena,
                                           absl::string_view field);

const absl::Status* CreateNoSuchKeyError(google::protobuf::Arena* arena,
                                         absl::string_view key);

const absl::Status* CreateUnknownValueError(google::protobuf::Arena* arena,
                                            absl::string_view unknown_path);

const absl::Status* CreateMissingAttributeError(
    google::protobuf::Arena* arena, absl::string_view missing_attribute_path);

const absl::Status* CreateUnknownFunctionResultError(
    google::protobuf::Arena* arena, absl::string_view help_message);

const absl::Status* CreateError(
    google::protobuf::Arena* arena, absl::string_view message,
    absl::StatusCode code = absl::StatusCode::kUnknown);

}  // namespace interop_internal
}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_ERRORS_H_
