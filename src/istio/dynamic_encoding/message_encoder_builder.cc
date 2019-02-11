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
#include <string>
#include <vector>

#include "message_encoder.h"
#include "message_encoder_builder.h"
#include "primitive_encoder.h"
#include "util.h"

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/stubs/statusor.h"
#include "policy/v1beta1/value_type.pb.h"

namespace istio {
namespace dynamic_encoding {
namespace {
using ::google::protobuf::Descriptor;
using ::google::protobuf::EnumDescriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::StatusOr;
using ::google::protobuf::util::error::INTERNAL;
// transformQuotedString removes quotes from strings and returns true
// if quotes were removed. constant strings are not expressions.
bool transformQuotedString(absl::any* value) {
  std::string* s = absl::any_cast<std::string>(value);
  if (s == nullptr) {
    return false;
  }

  if (s->size() < 2) {
    return false;
  }

  if ((s->substr(0, 1) == "'" && s->substr(s->size() - 1, 1) == "'") ||
      (s->substr(0, 1) == "\"" && s->substr(s->size() - 1, 1) == "\"")) {
    s->erase(0, 1);
    s->erase(s->size() - 1, 1);
    return true;
  }

  return false;
}
}  // namespace

MessageEncoderBuilder::MessageEncoderBuilder(
    const FileDescriptorSet* file_descriptor_set, bool skipUnknown)
    : skipUnknown_(skipUnknown) {
  resolver_ = absl::make_unique<Resolver>(file_descriptor_set);
  compiler_ = absl::make_unique<Compiler>();
}

std::unique_ptr<Encoder> MessageEncoderBuilder::Build(
    std::string msgName, absl::flat_hash_map<std::string, absl::any> data) {
  if (resolver_.get() == nullptr) {
    return nullptr;
  }
  const Descriptor* descriptor = resolver_->ResolveMessage(msgName);
  if (descriptor == nullptr) {
    return nullptr;
  }
  return buildMessage(descriptor, data, 0);
}

std::unique_ptr<Encoder> MessageEncoderBuilder::buildMessage(
    const Descriptor* msgDescriptor,
    absl::flat_hash_map<std::string, absl::any> data, int index) {
  std::unique_ptr<MessageEncoder> messageEncoder(
      new MessageEncoder(msgDescriptor, index));

  for (absl::flat_hash_map<std::string, absl::any>::iterator it = data.begin();
       it != data.end(); ++it) {
    const FieldDescriptor* field_descriptor =
        msgDescriptor->FindFieldByName(it->first);
    if (field_descriptor == nullptr) {
      if (skipUnknown_) continue;
      return nullptr;
    }

    bool noExpr = field_descriptor->is_map() && it->first == "key";
    auto* encoder = absl::any_cast<Encoder*>(&it->second);
    if (encoder != nullptr) {
      messageEncoder->AddFieldEncoder(std::unique_ptr<Encoder>(*encoder),
                                      field_descriptor);
      continue;
    }

    std::unique_ptr<std::vector<absl::any*>> any_array_ptr;
    std::vector<absl::any*>* any_array =
        absl::any_cast<std::vector<absl::any*>>(&it->second);
    if (any_array == nullptr) {
      any_array_ptr = absl::make_unique<std::vector<absl::any*>>();
      any_array_ptr->push_back(&it->second);
    } else {
      any_array_ptr.reset(any_array);
    }

    if (field_descriptor->type() ==
        ::google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
      buildMessageField(any_array_ptr.get(), messageEncoder.get(),
                        field_descriptor);

    } else {
      buildPrimitiveField(any_array_ptr.get(), messageEncoder.get(),
                          field_descriptor, noExpr);
    }
  }
  return std::move(messageEncoder);
}

Status MessageEncoderBuilder::buildPrimitiveField(
    std::vector<absl::any*>* any_array, MessageEncoder* messageEncoder,
    const FieldDescriptor* field_descriptor, bool noExpr) {
  const EnumDescriptor* enumDescriptor =
      resolver_->ResolveEnum(field_descriptor->type_name());
  if (field_descriptor->type() == FieldDescriptor::TYPE_ENUM &&
      enumDescriptor == nullptr) {
    const std::string err_msg = absl::StrFormat(
        "unable to resolve enum: %s for field %s",
        field_descriptor->type_name(), field_descriptor->name());
    return Status(INTERNAL, err_msg);
  }

  int index = -1;
  for (std::vector<absl::any*>::iterator it = any_array->begin();
       it != any_array->end(); ++it, ++index) {
    absl::any* any_value = *it;
    bool isConstString = transformQuotedString(any_value);
    bool isString = false;
    std::string* str = absl::any_cast<std::string>(any_value);
    if (str != nullptr) {
      isString = true;
    }
    if (noExpr || isConstString || !isString) {
      auto status =
          EncodeStaticField(any_value, messageEncoder, field_descriptor, index);
      if (!status.ok()) {
        return status;
      }
    } else {
      auto status_or_encoder =
          buildDynamicEncoder(*str, field_descriptor, enumDescriptor, index);
      if (!status_or_encoder.ok()) {
        return status_or_encoder.status();
      }
      std::unique_ptr<Encoder> encoder_ptr(status_or_encoder.ValueOrDie());
      messageEncoder->AddFieldEncoder(std::move(encoder_ptr), field_descriptor);
    }
  }
  return Status::OK;
}

::google::protobuf::util::Status MessageEncoderBuilder::buildMessageField(
    std::vector<absl::any*>* any_array, MessageEncoder* msgEncoder,
    const ::google::protobuf::FieldDescriptor* field_descriptor) {
  if (field_descriptor->type() !=
      ::google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
    return GetFieldEncodingError(field_descriptor);
  }

  int index = -1;
  for (std::vector<absl::any*>::iterator it = any_array->begin();
       it != any_array->end(); ++it, ++index) {
    absl::any* any_value = *it;
    absl::flat_hash_map<std::string, absl::any>* any_map =
        absl::any_cast<absl::flat_hash_map<std::string, absl::any>>(any_value);
    if (any_map == nullptr) {
      return GetFieldEncodingError(field_descriptor);
    }

    auto encoder_ptr =
        buildMessage(field_descriptor->message_type(), *any_map, index);
    msgEncoder->AddFieldEncoder(std::move(encoder_ptr), field_descriptor);
  }
  return Status::OK;
}

::google::protobuf::util::StatusOr<Encoder*>
MessageEncoderBuilder::buildDynamicEncoder(
    std::string value,
    const ::google::protobuf::FieldDescriptor* field_descriptor,
    const ::google::protobuf::EnumDescriptor* enumDescriptor, int index) {
  std::string compiled_expr;
  auto status_or_valuetype = compiler_->Compile(value, compiled_expr);
  if (!status_or_valuetype.ok()) {
    return status_or_valuetype.status();
  }
  istio::policy::v1beta1::ValueType value_type =
      status_or_valuetype.ValueOrDie();

  PrimitiveEncoder* encoder = PrimitiveEncoder::GetPrimitiveEncoder(
      field_descriptor, compiled_expr, index);
  // Validate Value Types returned for enum field.
  if (field_descriptor->type() ==
          ::google::protobuf::FieldDescriptor::TYPE_ENUM &&
      !(value_type == istio::policy::v1beta1::STRING ||
        value_type == istio::policy::v1beta1::INT64)) {
    return GetFieldEncodingError(field_descriptor);
  }
  if (field_descriptor->type() !=
          ::google::protobuf::FieldDescriptor::TYPE_ENUM &&
      value_type != encoder->AcceptsType()) {
    return GetFieldEncodingError(field_descriptor);
  }

  return encoder;
}

}  // namespace dynamic_encoding
}  // namespace istio
