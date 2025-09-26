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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_INTERNAL_FIELD_BACKED_LIST_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_INTERNAL_FIELD_BACKED_LIST_IMPL_H_

#include <utility>

#include "eval/public/cel_value.h"
#include "eval/public/structs/protobuf_value_factory.h"

namespace google::api::expr::runtime::internal {

// CelList implementation that uses "repeated" message field
// as backing storage.
//
// The internal implementation allows for interface updates without breaking
// clients that depend on this class for implementing custom CEL lists
class FieldBackedListImpl : public CelList {
 public:
  // message contains the "repeated" field
  // descriptor FieldDescriptor for the field
  FieldBackedListImpl(const google::protobuf::Message* message,
                      const google::protobuf::FieldDescriptor* descriptor,
                      ProtobufValueFactory factory, google::protobuf::Arena* arena)
      : message_(message),
        descriptor_(descriptor),
        reflection_(message_->GetReflection()),
        factory_(std::move(factory)),
        arena_(arena) {}

  // List size.
  int size() const override;

  // List element access operator.
  CelValue operator[](int index) const override;

 private:
  const google::protobuf::Message* message_;
  const google::protobuf::FieldDescriptor* descriptor_;
  const google::protobuf::Reflection* reflection_;
  ProtobufValueFactory factory_;
  google::protobuf::Arena* arena_;
};

}  // namespace google::api::expr::runtime::internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_INTERNAL_FIELD_BACKED_LIST_IMPL_H_
