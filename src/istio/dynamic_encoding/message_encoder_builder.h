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

#ifndef ISTIO_DYNAMIC_ENCODING_MESSAGE_ENCODER_BUILDER_H
#define ISTIO_DYNAMIC_ENCODING_MESSAGE_ENCODER_BUILDER_H
#include <string>

#include "compiler.h"
#include "encoder.h"
#include "message_encoder.h"
#include "resolver.h"

#include "absl/container/flat_hash_map.h"
#include "absl/types/any.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/message.h"
#include "google/protobuf/stubs/statusor.h"
#include "policy/v1beta1/type.pb.h"
#include "policy/v1beta1/value_type.pb.h"

namespace istio {
namespace dynamic_encoding {

class MessageEncoderBuilder {
 public:
  explicit MessageEncoderBuilder(
      const ::google::protobuf::FileDescriptorSet* file_descriptor_set) {
    MessageEncoderBuilder(file_descriptor_set, true);
  }
  MessageEncoderBuilder(
      const ::google::protobuf::FileDescriptorSet* file_descriptor_set,
      bool skipUnknown);
  // virtual destrutor
  virtual ~MessageEncoderBuilder() {}

  std::unique_ptr<Encoder> Build(
      std::string msgName, absl::flat_hash_map<std::string, absl::any> data);

 private:
  std::unique_ptr<Encoder> buildMessage(
      const ::google::protobuf::Descriptor*,
      absl::flat_hash_map<std::string, absl::any> data, int index);
  std::unique_ptr<Resolver> resolver_;
  std::unique_ptr<Compiler> compiler_;
  bool skipUnknown_;

  ::google::protobuf::util::Status buildPrimitiveField(
      std::vector<absl::any*>* any_array, MessageEncoder* msgEncoder,
      const ::google::protobuf::FieldDescriptor* field_descriptor, bool noExpr);

  ::google::protobuf::util::Status buildMessageField(
      std::vector<absl::any*>* any_array, MessageEncoder* msgEncoder,
      const ::google::protobuf::FieldDescriptor* field_descriptor);

  ::google::protobuf::util::StatusOr<Encoder*> buildDynamicEncoder(
      std::string value,
      const ::google::protobuf::FieldDescriptor* field_descriptor,
      const ::google::protobuf::EnumDescriptor* enumDescriptor, int index);
};
}  // namespace dynamic_encoding
}  // namespace istio

#endif  // ISTIO_DYNAMIC_ENCODING_MESSAGE_ENCODER_BUILDER_H
