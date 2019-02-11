/* Copyright 2018 Istio Authors. All Rights Reserved.
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

#ifndef ISTIO_DYNAMIC_ENCODING_FIELD_ENCODER_H
#define ISTIO_DYNAMIC_ENCODING_FIELD_ENCODER_H

#include <memory>
#include <string>

#include "encoder.h"

#include "absl/types/any.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/stubs/statusor.h"

namespace istio {
namespace dynamic_encoding {

class FieldEncoder : public Encoder {
 private:
  // packed fields have a list of encoders.
  // non-packed fields have one.
  std::set<std::unique_ptr<Encoder>> encoders;

  // number fields are sorted by fieldEncoder number.
  int number;
  // name for debug.
  std::string name;

  // dynamic expr to be evaluated at runtime.
  std::string expr;
  const ::google::protobuf::FieldDescriptor* fieldDescriptor;

  const ::google::protobuf::EnumDescriptor* enumDescriptor;

 public:
  FieldEncoder() {}
  ~FieldEncoder() {}

  ::google::protobuf::util::StatusOr<std::string> Encode() {
    return ::google::protobuf::util::Status::OK;
  }

  const ::google::protobuf::FieldDescriptor* GetFieldDescriptor() {
    return fieldDescriptor;
  }

  const int GetIndex() { return number; }

  const ::google::protobuf::EnumDescriptor* GetEnumDescriptor() {
    return enumDescriptor;
  }
};

}  // namespace dynamic_encoding
}  // namespace istio

#endif  // ISTIO_DYNAMIC_ENCODING_FIELD_ENCODER_H
