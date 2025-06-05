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

#include "extensions/protobuf/internal/constant.h"

#include <cstddef>
#include <cstdint>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "common/constant.h"
#include "internal/proto_time_encoding.h"

namespace cel::extensions::protobuf_internal {

using ConstantProto = google::api::expr::v1alpha1::Constant;

absl::Status ConstantToProto(const Constant& constant,
                             absl::Nonnull<ConstantProto*> proto) {
  return absl::visit(absl::Overload(
                         [proto](absl::monostate) -> absl::Status {
                           proto->clear_constant_kind();
                           return absl::OkStatus();
                         },
                         [proto](std::nullptr_t) -> absl::Status {
                           proto->set_null_value(google::protobuf::NULL_VALUE);
                           return absl::OkStatus();
                         },
                         [proto](bool value) -> absl::Status {
                           proto->set_bool_value(value);
                           return absl::OkStatus();
                         },
                         [proto](int64_t value) -> absl::Status {
                           proto->set_int64_value(value);
                           return absl::OkStatus();
                         },
                         [proto](uint64_t value) -> absl::Status {
                           proto->set_uint64_value(value);
                           return absl::OkStatus();
                         },
                         [proto](double value) -> absl::Status {
                           proto->set_double_value(value);
                           return absl::OkStatus();
                         },
                         [proto](const BytesConstant& value) -> absl::Status {
                           proto->set_bytes_value(value);
                           return absl::OkStatus();
                         },
                         [proto](const StringConstant& value) -> absl::Status {
                           proto->set_string_value(value);
                           return absl::OkStatus();
                         },
                         [proto](absl::Duration value) -> absl::Status {
                           return internal::EncodeDuration(
                               value, proto->mutable_duration_value());
                         },
                         [proto](absl::Time value) -> absl::Status {
                           return internal::EncodeTime(
                               value, proto->mutable_timestamp_value());
                         }),
                     constant.kind());
}

absl::Status ConstantFromProto(const ConstantProto& proto, Constant& constant) {
  switch (proto.constant_kind_case()) {
    case ConstantProto::CONSTANT_KIND_NOT_SET:
      constant = Constant{};
      break;
    case ConstantProto::kNullValue:
      constant.set_null_value();
      break;
    case ConstantProto::kBoolValue:
      constant.set_bool_value(proto.bool_value());
      break;
    case ConstantProto::kInt64Value:
      constant.set_int_value(proto.int64_value());
      break;
    case ConstantProto::kUint64Value:
      constant.set_uint_value(proto.uint64_value());
      break;
    case ConstantProto::kDoubleValue:
      constant.set_double_value(proto.double_value());
      break;
    case ConstantProto::kStringValue:
      constant.set_string_value(proto.string_value());
      break;
    case ConstantProto::kBytesValue:
      constant.set_bytes_value(proto.bytes_value());
      break;
    case ConstantProto::kDurationValue:
      constant.set_duration_value(
          internal::DecodeDuration(proto.duration_value()));
      break;
    case ConstantProto::kTimestampValue:
      constant.set_timestamp_value(
          internal::DecodeTime(proto.timestamp_value()));
      break;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected ConstantKindCase: ",
                       static_cast<int>(proto.constant_kind_case())));
  }
  return absl::OkStatus();
}

}  // namespace cel::extensions::protobuf_internal
