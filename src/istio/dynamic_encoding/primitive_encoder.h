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

#ifndef ISTIO_DYNAMIC_ENCODING_PRIMITIVE_ENCODER_H
#define ISTIO_DYNAMIC_ENCODING_PRIMITIVE_ENCODER_H
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>

#include "encoder.h"

#include "absl/container/flat_hash_map.h"
#include "absl/types/any.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/stubs/statusor.h"
#include "policy/v1beta1/type.pb.h"
#include "policy/v1beta1/value_type.pb.h"

namespace istio {
namespace dynamic_encoding {

class PrimitiveEncoder : public Encoder {
 public:
  static PrimitiveEncoder* GetPrimitiveEncoder(
      const ::google::protobuf::FieldDescriptor*, std::string compiled_expr,
      int index);
  ~PrimitiveEncoder() {}

  ::google::protobuf::util::StatusOr<absl::any> Encode() = 0;

  virtual istio::policy::v1beta1::ValueType AcceptsType() = 0;

  void SetAttributeBag(
      const absl::flat_hash_map<std::string, absl::any>* attribute_bag) {
    if (attribute_bag == nullptr) return;
    attribute_bag_ = attribute_bag;
  }

  const ::google::protobuf::FieldDescriptor* GetFieldDescriptor() {
    return field_descriptor_;
  }

  const int GetIndex() { return index_; }

  PrimitiveEncoder(const ::google::protobuf::FieldDescriptor* field_descriptor,
                   std::string compiled_expr, int index)
      : field_descriptor_(field_descriptor),
        compiled_expr_(compiled_expr),
        index_(index) {}

 protected:
  const ::google::protobuf::FieldDescriptor* field_descriptor_;
  // compiled dynamic expr to be evaluated at runtime.
  std::string compiled_expr_;
  // number fields are sorted by fieldEncoder number.
  int index_;
  const absl::flat_hash_map<std::string, absl::any>* attribute_bag_;
};

class StringTypeEncoder : public PrimitiveEncoder {
 public:
  StringTypeEncoder(const ::google::protobuf::FieldDescriptor* field_descriptor,
                    std::string compiled_expr, int index)
      : PrimitiveEncoder(field_descriptor, compiled_expr, index) {}

  ::google::protobuf::util::StatusOr<absl::any> Encode();

  istio::policy::v1beta1::ValueType AcceptsType() {
    return istio::policy::v1beta1::STRING;
  }
};

class DoubleTypeEncoder : public PrimitiveEncoder {
 public:
  DoubleTypeEncoder(const ::google::protobuf::FieldDescriptor* field_descriptor,
                    std::string compiled_expr, int index)
      : PrimitiveEncoder(field_descriptor, compiled_expr, index) {}

  ::google::protobuf::util::StatusOr<absl::any> Encode();

  istio::policy::v1beta1::ValueType AcceptsType() {
    return istio::policy::v1beta1::DOUBLE;
  }
};

class Int64TypeEncoder : public PrimitiveEncoder {
 public:
  Int64TypeEncoder(const ::google::protobuf::FieldDescriptor* field_descriptor,
                   std::string compiled_expr, int index)
      : PrimitiveEncoder(field_descriptor, compiled_expr, index) {}

  ::google::protobuf::util::StatusOr<absl::any> Encode();

  istio::policy::v1beta1::ValueType AcceptsType() {
    return istio::policy::v1beta1::INT64;
  }
};

class BoolTypeEncoder : public PrimitiveEncoder {
 public:
  BoolTypeEncoder(const ::google::protobuf::FieldDescriptor* field_descriptor,
                  std::string compiled_expr, int index)
      : PrimitiveEncoder(field_descriptor, compiled_expr, index) {}

  ::google::protobuf::util::StatusOr<absl::any> Encode();

  istio::policy::v1beta1::ValueType AcceptsType() {
    return istio::policy::v1beta1::BOOL;
  }
};

class EnumTypeEncoder : public PrimitiveEncoder {
 public:
  EnumTypeEncoder(const ::google::protobuf::FieldDescriptor* field_descriptor,
                  std::string compiled_expr, int index)
      : PrimitiveEncoder(field_descriptor, compiled_expr, index) {}

  ::google::protobuf::util::StatusOr<absl::any> Encode();

  istio::policy::v1beta1::ValueType AcceptsType() {
    return istio::policy::v1beta1::VALUE_TYPE_UNSPECIFIED;
  }
};

}  // namespace dynamic_encoding
}  // namespace istio

#endif  // ISTIO_DYNAMIC_ENCODING_PRIMITIVE_ENCODER_H
