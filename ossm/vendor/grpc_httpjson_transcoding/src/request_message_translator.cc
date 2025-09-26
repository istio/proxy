// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "grpc_transcoding/request_message_translator.h"

#include <string>

#include "google/protobuf/stubs/bytestream.h"
#include "google/protobuf/util/converter/error_listener.h"
#include "google/protobuf/util/converter/protostream_objectwriter.h"
#include "grpc_transcoding/prefix_writer.h"
#include "grpc_transcoding/request_weaver.h"

namespace pbconv = ::google::protobuf::util::converter;

namespace google {
namespace grpc {

namespace transcoding {

namespace {

pbconv::ProtoStreamObjectWriter::Options GetProtoWriterOptions(
    bool case_insensitive_enum_parsing) {
  auto options = pbconv::ProtoStreamObjectWriter::Options::Defaults();
  // Don't fail the translation if there are unknown fields in JSON.
  // This will make sure that we allow backward and forward compatible APIs.
  options.ignore_unknown_fields = true;
  options.case_insensitive_enum_parsing = case_insensitive_enum_parsing;
  return options;
}

}  // namespace

RequestMessageTranslator::RequestMessageTranslator(
    google::protobuf::util::TypeResolver& type_resolver, bool output_delimiter,
    RequestInfo request_info)
    : message_(),
      sink_(&message_),
      error_listener_(),
      proto_writer_(
          &type_resolver, *request_info.message_type, &sink_, &error_listener_,
          GetProtoWriterOptions(request_info.case_insensitive_enum_parsing)),
      request_weaver_(),
      prefix_writer_(),
      writer_pipeline_(&proto_writer_),
      output_delimiter_(output_delimiter),
      finished_(false) {
  // Relax Base64 decoding to support RFC 2045 Base64
  proto_writer_.set_use_strict_base64_decoding(false);

  // Create a RequestWeaver if we have variable bindings to weave
  if (!request_info.variable_bindings.empty()) {
    request_weaver_.reset(new RequestWeaver(
        std::move(request_info.variable_bindings), writer_pipeline_,
        &error_listener_, request_info.reject_binding_body_field_collisions));
    writer_pipeline_ = request_weaver_.get();
  }

  // Create a PrefixWriter if there is a prefix to write
  if (!request_info.body_field_path.empty() &&
      "*" != request_info.body_field_path) {
    prefix_writer_.reset(
        new PrefixWriter(request_info.body_field_path, writer_pipeline_));
    writer_pipeline_ = prefix_writer_.get();
  }

  if (output_delimiter_) {
    // Reserve space for the delimiter at the begining of the message_
    ReserveDelimiterSpace();
  }
}

RequestMessageTranslator::~RequestMessageTranslator() {}

bool RequestMessageTranslator::Finished() const { return finished_; }

bool RequestMessageTranslator::NextMessage(std::string* message) {
  if (Finished()) {
    // Finished reading
    return false;
  }
  if (!proto_writer_.done()) {
    // No full message yet
    return false;
  }
  if (output_delimiter_) {
    WriteDelimiter();
  }
  *message = std::move(message_);
  finished_ = true;
  return true;
}

void RequestMessageTranslator::ReserveDelimiterSpace() {
  static char reserved[kDelimiterSize] = {0};
  sink_.Append(reserved, sizeof(reserved));
}

namespace {

void SizeToDelimiter(unsigned size, unsigned char* delimiter) {
  delimiter[0] = 0;  // compression bit

  // big-endian 32-bit length
  delimiter[4] = 0xFF & size;
  size >>= 8;
  delimiter[3] = 0xFF & size;
  size >>= 8;
  delimiter[2] = 0xFF & size;
  size >>= 8;
  delimiter[1] = 0xFF & size;
}

}  // namespace

void RequestMessageTranslator::WriteDelimiter() {
  // Asumming that the message_.size() - kDelimiterSize is less than UINT_MAX
  SizeToDelimiter(static_cast<unsigned>(message_.size() - kDelimiterSize),
                  reinterpret_cast<unsigned char*>(&message_[0]));
}

}  // namespace transcoding

}  // namespace grpc
}  // namespace google
