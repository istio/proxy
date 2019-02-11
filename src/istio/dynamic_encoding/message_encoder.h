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

#ifndef ISTIO_DYNAMIC_ENCODING_MESSAGE_ENCODER_H
#define ISTIO_DYNAMIC_ENCODING_MESSAGE_ENCODER_H

#include <memory>
#include <set>
#include <string>

#include "encoder.h"
#include "istio_message.h"

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/stubs/statusor.h"

namespace istio {
namespace dynamic_encoding {

class MessageEncoder : public Encoder {
 public:
  MessageEncoder(const ::google::protobuf::Descriptor* msgDescriptor, int index)
      : descriptor_(msgDescriptor), number_(index) {
    dynamic_message_fatory_ =
        absl::make_unique<google::protobuf::DynamicMessageFactory>();
    std::unique_ptr<::google::protobuf::Message> message(
        dynamic_message_fatory_->GetPrototype(msgDescriptor)->New());
    msg_ = absl::make_unique<IstioMessage>(std::move(message));
  }
  ~MessageEncoder() {
    msg_ = nullptr;
    dynamic_message_fatory_ = nullptr;
  }

  ::google::protobuf::util::StatusOr<absl::any> Encode();

  void SetAttributeBag(
      const absl::flat_hash_map<std::string, absl::any>* attribute_bag) {
    if (attribute_bag == nullptr) return;
    attributeBag_ = attribute_bag;
  }

  const ::google::protobuf::Reflection* GetReflection() {
    return msg_->message()->GetReflection();
  }

  ::google::protobuf::Message* GetMessage() { return msg_->message(); }

  void AddFieldEncoder(
      std::unique_ptr<Encoder> encoder,
      const ::google::protobuf::FieldDescriptor* fieldDescriptor) {
    fields_.emplace(std::make_pair(std::move(encoder), fieldDescriptor));
  }

  const ::google::protobuf::Descriptor* GetDescriptor() { return descriptor_; }

  const int GetIndex() { return number_; }

  ::google::protobuf::util::StatusOr<std::string> EncodeBytes();

 private:
  std::set<std::pair<std::unique_ptr<Encoder>,
                     const ::google::protobuf::FieldDescriptor*>>
      fields_;
  std::unique_ptr<IstioMessage> msg_;
  std::unique_ptr<google::protobuf::DynamicMessageFactory>
      dynamic_message_fatory_;
  const ::google::protobuf::Descriptor* descriptor_;
  // number fields are sorted by fieldEncoder number.
  int number_;
  absl::flat_hash_map<std::string, absl::any> compiled_exprToValueMap_;

  const absl::flat_hash_map<std::string, absl::any>* attributeBag_;
};

}  // namespace dynamic_encoding
}  // namespace istio

#endif  // ISTIO_DYNAMIC_ENCODING_MESSAGE_ENCODER_H
