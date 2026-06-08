// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/protobuf/util/converter/legacy_json_util.h"

#include <string>

#include "absl/base/call_once.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_sink.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/stubs/bytestream.h"
#include "google/protobuf/stubs/status_macros.h"
#include "google/protobuf/util/converter/default_value_objectwriter.h"
#include "google/protobuf/util/converter/error_listener.h"
#include "google/protobuf/util/converter/json_objectwriter.h"
#include "google/protobuf/util/converter/json_stream_parser.h"
#include "google/protobuf/util/converter/protostream_objectsource.h"
#include "google/protobuf/util/converter/protostream_objectwriter.h"
#include "google/protobuf/util/type_resolver.h"
#include "google/protobuf/util/type_resolver_util.h"

namespace protobuf {
namespace util {
using ::google::protobuf::io::zc_sink_internal::ZeroCopyStreamByteSink;
namespace legacy_json_util {

absl::Status BinaryToJsonStream(TypeResolver* resolver,
                                const std::string& type_url,
                                io::ZeroCopyInputStream* binary_input,
                                io::ZeroCopyOutputStream* json_output,
                                const JsonPrintOptions& options) {
  io::CodedInputStream in_stream(binary_input);
  google::protobuf::Type type;
  RETURN_IF_ERROR(resolver->ResolveMessageType(type_url, &type));
  converter::ProtoStreamObjectSource::RenderOptions render_options;
  render_options.use_ints_for_enums = options.always_print_enums_as_ints;
  render_options.preserve_proto_field_names =
      options.preserve_proto_field_names;
  converter::ProtoStreamObjectSource proto_source(&in_stream, resolver, type,
                                                  render_options);
  io::CodedOutputStream out_stream(json_output);
  converter::JsonObjectWriter json_writer(options.add_whitespace ? " " : "",
                                          &out_stream);
  if (options.always_print_primitive_fields) {
    converter::DefaultValueObjectWriter default_value_writer(resolver, type,
                                                             &json_writer);
    default_value_writer.set_preserve_proto_field_names(
        options.preserve_proto_field_names);
    default_value_writer.set_print_enums_as_ints(
        options.always_print_enums_as_ints);
    return proto_source.WriteTo(&default_value_writer);
  } else {
    return proto_source.WriteTo(&json_writer);
  }
}

absl::Status BinaryToJsonString(TypeResolver* resolver,
                                const std::string& type_url,
                                const std::string& binary_input,
                                std::string* json_output,
                                const JsonPrintOptions& options) {
  io::ArrayInputStream input_stream(binary_input.data(), binary_input.size());
  io::StringOutputStream output_stream(json_output);
  return BinaryToJsonStream(resolver, type_url, &input_stream, &output_stream,
                            options);
}

namespace {
class StatusErrorListener : public converter::ErrorListener {
 public:
  StatusErrorListener() {}
  StatusErrorListener(const StatusErrorListener&) = delete;
  StatusErrorListener& operator=(const StatusErrorListener&) = delete;
  ~StatusErrorListener() override {}

  absl::Status GetStatus() { return status_; }

  void InvalidName(const converter::LocationTrackerInterface& loc,
                   absl::string_view unknown_name,
                   absl::string_view message) override {
    std::string loc_string = GetLocString(loc);
    if (!loc_string.empty()) {
      loc_string.append(" ");
    }
    status_ = absl::InvalidArgumentError(
        absl::StrCat(loc_string, unknown_name, ": ", message));
  }

  void InvalidValue(const converter::LocationTrackerInterface& loc,
                    absl::string_view type_name,
                    absl::string_view value) override {
    status_ = absl::InvalidArgumentError(absl::StrCat(
        GetLocString(loc), ": invalid value ", value, " for type ", type_name));
  }

  void MissingField(const converter::LocationTrackerInterface& loc,
                    absl::string_view missing_name) override {
    status_ = absl::InvalidArgumentError(
        absl::StrCat(GetLocString(loc), ": missing field ", missing_name));
  }

 private:
  absl::Status status_;

  std::string GetLocString(const converter::LocationTrackerInterface& loc) {
    std::string loc_string = loc.ToString();
    absl::StripAsciiWhitespace(&loc_string);
    if (!loc_string.empty()) {
      loc_string = absl::StrCat("(", loc_string, ")");
    }
    return loc_string;
  }
};

// Wrapper to convert ZeroCopyStreamByteSink into a proper ByteSink.  This is
// necessary because ByteSink hasn't been open-sourced.
class ZeroCopyStreamByteSinkWrapper : public strings::ByteSink {
 public:
  explicit ZeroCopyStreamByteSinkWrapper(io::ZeroCopyOutputStream* stream)
      : sink_(stream) {}
  ZeroCopyStreamByteSinkWrapper(const ZeroCopyStreamByteSink&) = delete;
  ZeroCopyStreamByteSinkWrapper& operator=(const ZeroCopyStreamByteSink&) =
      delete;

  void Append(const char* bytes, size_t len) override {
    sink_.Append(bytes, len);
  }

 private:
  ZeroCopyStreamByteSink sink_;
};
}  // namespace

absl::Status JsonToBinaryStream(TypeResolver* resolver,
                                const std::string& type_url,
                                io::ZeroCopyInputStream* json_input,
                                io::ZeroCopyOutputStream* binary_output,
                                const JsonParseOptions& options) {
  google::protobuf::Type type;
  RETURN_IF_ERROR(resolver->ResolveMessageType(type_url, &type));
  ZeroCopyStreamByteSinkWrapper sink(binary_output);
  StatusErrorListener listener;
  converter::ProtoStreamObjectWriter::Options proto_writer_options;
  proto_writer_options.ignore_unknown_fields = options.ignore_unknown_fields;
  proto_writer_options.ignore_unknown_enum_values =
      options.ignore_unknown_fields;
  proto_writer_options.case_insensitive_enum_parsing =
      options.case_insensitive_enum_parsing;
  converter::ProtoStreamObjectWriter proto_writer(
      resolver, type, &sink, &listener, proto_writer_options);
  proto_writer.set_use_strict_base64_decoding(false);

  converter::JsonStreamParser parser(&proto_writer);
  const void* buffer;
  int length;
  while (json_input->Next(&buffer, &length)) {
    if (length == 0) continue;
    RETURN_IF_ERROR(parser.Parse(
        absl::string_view(static_cast<const char*>(buffer), length)));
  }
  RETURN_IF_ERROR(parser.FinishParse());

  return listener.GetStatus();
}

absl::Status JsonToBinaryString(TypeResolver* resolver,
                                const std::string& type_url,
                                absl::string_view json_input,
                                std::string* binary_output,
                                const JsonParseOptions& options) {
  io::ArrayInputStream input_stream(json_input.data(), json_input.size());
  io::StringOutputStream output_stream(binary_output);
  return JsonToBinaryStream(resolver, type_url, &input_stream, &output_stream,
                            options);
}

namespace {
constexpr absl::string_view kTypeUrlPrefix = "type.googleapis.com";
TypeResolver* generated_type_resolver_ = nullptr;
absl::once_flag generated_type_resolver_init_;

std::string GetTypeUrl(const Message& message) {
  return absl::StrCat(kTypeUrlPrefix, "/",
                      message.GetDescriptor()->full_name());
}

void DeleteGeneratedTypeResolver() {  // NOLINT
  delete generated_type_resolver_;
}

void InitGeneratedTypeResolver() {
  generated_type_resolver_ = NewTypeResolverForDescriptorPool(
      kTypeUrlPrefix, DescriptorPool::generated_pool());
  ::google::protobuf::internal::OnShutdown(&DeleteGeneratedTypeResolver);
}

TypeResolver* GetGeneratedTypeResolver() {
  absl::call_once(generated_type_resolver_init_, InitGeneratedTypeResolver);
  return generated_type_resolver_;
}
}  // namespace

absl::Status MessageToJsonString(const Message& message, std::string* output,
                                 const JsonOptions& options) {
  const DescriptorPool* pool = message.GetDescriptor()->file()->pool();
  TypeResolver* resolver =
      pool == DescriptorPool::generated_pool()
          ? GetGeneratedTypeResolver()
          : NewTypeResolverForDescriptorPool(kTypeUrlPrefix, pool);
  absl::Status result =
      BinaryToJsonString(resolver, GetTypeUrl(message),
                         message.SerializeAsString(), output, options);
  if (pool != DescriptorPool::generated_pool()) {
    delete resolver;
  }
  return result;
}

absl::Status JsonStringToMessage(absl::string_view input, Message* message,
                                 const JsonParseOptions& options) {
  const DescriptorPool* pool = message->GetDescriptor()->file()->pool();
  TypeResolver* resolver =
      pool == DescriptorPool::generated_pool()
          ? GetGeneratedTypeResolver()
          : NewTypeResolverForDescriptorPool(kTypeUrlPrefix, pool);
  std::string binary;
  absl::Status result = JsonToBinaryString(resolver, GetTypeUrl(*message),
                                           input, &binary, options);
  if (result.ok() && !message->ParseFromString(binary)) {
    result = absl::InvalidArgumentError(
        "JSON transcoder produced invalid protobuf output.");
  }
  if (pool != DescriptorPool::generated_pool()) {
    delete resolver;
  }
  return result;
}

}  // namespace legacy_json_util
}  // namespace util
}  // namespace protobuf
}  // namespace google
