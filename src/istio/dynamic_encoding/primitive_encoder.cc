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
#include <memory>

#include "evaluator.h"
#include "primitive_encoder.h"
#include "util.h"

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
#include "absl/types/any.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/stubs/status.h"
#include "google/protobuf/stubs/statusor.h"

namespace istio {
namespace dynamic_encoding {
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::util::Status;
using google::protobuf::util::error::INTERNAL;
using int64 = std::int64_t;
namespace {

absl::flat_hash_map<::google::protobuf::FieldDescriptor::CppType,
                    PrimitiveEncoder*>*
PrimitiveTypeEncoderMap(
    const ::google::protobuf::FieldDescriptor* field_descriptor,
    std::string compiled_expr, int index) {
  static absl::flat_hash_map<::google::protobuf::FieldDescriptor::CppType,
                             PrimitiveEncoder*>* primitive_encoder_map =
      [field_descriptor, compiled_expr, index]() {
        auto* primEncoderMap = new absl::flat_hash_map<
            ::google::protobuf::FieldDescriptor::CppType, PrimitiveEncoder*>();
        primEncoderMap->emplace(
            ::google::protobuf::FieldDescriptor::CPPTYPE_INT32,
            new Int64TypeEncoder(field_descriptor, compiled_expr, index));
        primEncoderMap->emplace(
            ::google::protobuf::FieldDescriptor::CPPTYPE_INT64,
            new Int64TypeEncoder(field_descriptor, compiled_expr, index));
        primEncoderMap->emplace(
            ::google::protobuf::FieldDescriptor::CPPTYPE_UINT32,
            new Int64TypeEncoder(field_descriptor, compiled_expr, index));
        primEncoderMap->emplace(
            ::google::protobuf::FieldDescriptor::CPPTYPE_UINT64,
            new Int64TypeEncoder(field_descriptor, compiled_expr, index));
        primEncoderMap->emplace(
            ::google::protobuf::FieldDescriptor::CPPTYPE_BOOL,
            new BoolTypeEncoder(field_descriptor, compiled_expr, index));
        primEncoderMap->emplace(
            ::google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE,
            new DoubleTypeEncoder(field_descriptor, compiled_expr, index));
        primEncoderMap->emplace(
            ::google::protobuf::FieldDescriptor::CPPTYPE_FLOAT,
            new DoubleTypeEncoder(field_descriptor, compiled_expr, index));
        primEncoderMap->emplace(
            ::google::protobuf::FieldDescriptor::CPPTYPE_ENUM,
            new EnumTypeEncoder(field_descriptor, compiled_expr, index));
        primEncoderMap->emplace(
            ::google::protobuf::FieldDescriptor::CPPTYPE_STRING,
            new StringTypeEncoder(field_descriptor, compiled_expr, index));
        return primEncoderMap;
      }();

  return primitive_encoder_map;
}

Status GetEvaluatorError(const std::string& compiled_expr,
                         const FieldDescriptor* field_descriptor) {
  const std::string err_msg = absl::StrFormat(
      "unable to evaluate: %s for field %s of type %s", compiled_expr,
      field_descriptor->type_name(), field_descriptor->name());
  return Status(INTERNAL, err_msg);
}

}  // namespace

PrimitiveEncoder* PrimitiveEncoder::GetPrimitiveEncoder(
    const ::google::protobuf::FieldDescriptor* field_descriptor,
    std::string compiled_expr, int index) {
  // if(field_descriptor->
  auto result = PrimitiveTypeEncoderMap(field_descriptor, compiled_expr, index)
                    ->find(field_descriptor->cpp_type());
  if (result !=
      PrimitiveTypeEncoderMap(field_descriptor, compiled_expr, index)->end()) {
    return result->second;
  }

  return nullptr;
}

::google::protobuf::util::StatusOr<absl::any> StringTypeEncoder::Encode() {
  auto status_or_value = Evaluator::Evaluate(&compiled_expr_, attribute_bag_);
  if (!status_or_value.ok()) {
    return status_or_value.status();
  }
  absl::any value = status_or_value.ValueOrDie();
  std::string* str = absl::any_cast<std::string>(&value);
  if (str == nullptr) {
    return GetEvaluatorError(compiled_expr_, field_descriptor_);
  }

  return value;
}

::google::protobuf::util::StatusOr<absl::any> DoubleTypeEncoder::Encode() {
  auto status_or_value = Evaluator::Evaluate(&compiled_expr_, attribute_bag_);
  if (!status_or_value.ok()) {
    return status_or_value.status();
  }
  absl::any value = status_or_value.ValueOrDie();
  double* double_value = absl::any_cast<double>(&value);
  if (double_value == nullptr) {
    return GetEvaluatorError(compiled_expr_, field_descriptor_);
  }

  return value;
}

::google::protobuf::util::StatusOr<absl::any> Int64TypeEncoder::Encode() {
  auto status_or_value = Evaluator::Evaluate(&compiled_expr_, attribute_bag_);
  if (!status_or_value.ok()) {
    return status_or_value.status();
  }
  absl::any value = status_or_value.ValueOrDie();
  int64* int64_value = absl::any_cast<int64>(&value);
  if (int64_value == nullptr) {
    return GetEvaluatorError(compiled_expr_, field_descriptor_);
  }

  return value;
}

::google::protobuf::util::StatusOr<absl::any> BoolTypeEncoder::Encode() {
  auto status_or_value = Evaluator::Evaluate(&compiled_expr_, attribute_bag_);
  if (!status_or_value.ok()) {
    return status_or_value.status();
  }
  absl::any value = status_or_value.ValueOrDie();
  bool* bool_value = absl::any_cast<bool>(&value);
  if (bool_value == nullptr) {
    return GetEvaluatorError(compiled_expr_, field_descriptor_);
  }

  return value;
}

::google::protobuf::util::StatusOr<absl::any> EnumTypeEncoder::Encode() {
  auto status_or_value = Evaluator::Evaluate(&compiled_expr_, attribute_bag_);
  if (!status_or_value.ok()) {
    return status_or_value.status();
  }
  absl::any value = status_or_value.ValueOrDie();

  status_or_value = GetEnumescriptorValue(&value, field_descriptor_);
  if (!status_or_value.ok()) {
    return status_or_value.status();
  }
  value = status_or_value.ValueOrDie();

  return value;
}

}  // namespace dynamic_encoding
}  // namespace istio
