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
//
// Converters to/from serialized Value to/from runtime values.
#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_VALUE_CONVERSION_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_VALUE_CONVERSION_H_

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/api/expr/v1alpha1/value.pb.h"
#include "cel/expr/value.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "common/value.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

// TODO(uncreated-issue/84): Clean up and expose cel::expr::Value converters
// in the common folder.
namespace cel::test {

ABSL_MUST_USE_RESULT
inline bool UnsafeConvertWireCompatProto(
    const google::protobuf::MessageLite& src, google::protobuf::MessageLite* absl_nonnull dest) {
  absl::Cord serialized;
  return src.SerializePartialToCord(&serialized) &&
         dest->ParsePartialFromCord(serialized);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const cel::expr::CheckedExpr& src,
    google::api::expr::v1alpha1::CheckedExpr* absl_nonnull dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const google::api::expr::v1alpha1::CheckedExpr& src,
    cel::expr::CheckedExpr* absl_nonnull dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const cel::expr::ParsedExpr& src,
    google::api::expr::v1alpha1::ParsedExpr* absl_nonnull dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const google::api::expr::v1alpha1::ParsedExpr& src,
    cel::expr::ParsedExpr* absl_nonnull dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const cel::expr::Expr& src,
    google::api::expr::v1alpha1::Expr* absl_nonnull dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(const google::api::expr::v1alpha1::Expr& src,
                                   cel::expr::Expr* absl_nonnull dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const cel::expr::Value& src,
    google::api::expr::v1alpha1::Value* absl_nonnull dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const google::api::expr::v1alpha1::Value& src,
    cel::expr::Value* absl_nonnull dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

absl::StatusOr<Value> FromExprValue(
    const cel::expr::Value& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena);

absl::StatusOr<cel::expr::Value> ToExprValue(
    const Value& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena);

}  // namespace cel::test
#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_VALUE_CONVERSION_H_
