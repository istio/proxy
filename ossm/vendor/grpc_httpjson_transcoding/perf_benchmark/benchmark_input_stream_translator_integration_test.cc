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
#include "perf_benchmark/benchmark_input_stream.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/log/absl_check.h"
#include "google/api/service.pb.h"
#include "google/protobuf/text_format.h"
#include "grpc_transcoding/json_request_translator.h"
#include "grpc_transcoding/response_to_json_translator.h"
#include "grpc_transcoding/type_helper.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "perf_benchmark/benchmark.pb.h"
#include "perf_benchmark/utils.h"

namespace google {
namespace grpc {
namespace transcoding {

namespace perf_benchmark {
namespace {
namespace pb = ::google::protobuf;

constexpr absl::string_view kServiceConfigTextProtoFile =
    "benchmark_service.textproto";

// Global type helper containing the type information of the benchmark_service
// service config object.
[[nodiscard]] const TypeHelper& GetBenchmarkTypeHelper() {
  static const auto* const kTypeHelper = [] {
    // Load service config proto into Service object.
    // Construct object on the heap using new without calling its dtor to
    // avoid destruction issue with static variables. However, this can cause
    // unnecessary heap allocations and create minor performance concerns.
    // For a small benchmark script, this is okay.
    auto* service = new google::api::Service();
    ABSL_CHECK_OK(
        LoadService(std::string(kServiceConfigTextProtoFile), service));

    // Create a TypeHelper based on the service config.
    // Construct object on the heap for the same reason as the Service config.
    auto* type_helper = new TypeHelper(service->types(), service->enums());
    return type_helper;
  }();
  return *kTypeHelper;
}

std::string ParseJsonMessageToProtoMessage(absl::string_view json_msg,
                                           absl::string_view msg_type,
                                           uint64_t num_checks,
                                           RequestInfo request_info = {}) {
  BenchmarkZeroCopyInputStream is(std::string(json_msg), num_checks);
  // Get message type from the global TypeHelper
  const TypeHelper& type_helper = GetBenchmarkTypeHelper();
  const pb::Type* type = type_helper.Info()->GetTypeByTypeUrl(
      absl::StrFormat("type.googleapis.com/%s", msg_type));

  // body field path used in this benchmark are all "*"
  request_info.body_field_path = "*";
  request_info.message_type = type;

  std::string message;
  JsonRequestTranslator translator(type_helper.Resolver(), &is, request_info,
                                   false, false);
  MessageStream& out = translator.Output();
  EXPECT_TRUE(out.Status().ok());

  // Read the message
  while (out.NextMessage(&message)) {
  }
  return message;
}

// class T protobuf class payload field type needs to support parsing 0 such as
// int32, string, and double.
template <class T>
void IntegrationWithJsonRequestTranslatorArrayProtoHelper(
    absl::string_view msg_type) {
  // JSON message containing an array of 3 zeros
  absl::string_view json_msg = R"({"payload":["0","0","0"]})";
  const uint64_t arr_length = 3;

  std::string proto_str = ParseJsonMessageToProtoMessage(json_msg, msg_type, 1);

  // Verification - array length should be 3
  T actual_proto;
  actual_proto.ParseFromString(proto_str);
  EXPECT_EQ(actual_proto.payload().size(), arr_length);
}

uint64_t GetNestedProtoLayer(const std::string& proto_msg) {
  NestedPayload actual_proto;
  actual_proto.ParseFromString(proto_msg);

  uint64_t actual_layers = 0;
  const NestedPayload* it = &actual_proto;
  while (it->has_nested()) {
    ++actual_layers;
    it = &(it->nested());
  }
  return actual_layers;
}

std::string GetNestedProtoValue(const std::string& proto_msg) {
  NestedPayload actual_proto;
  actual_proto.ParseFromString(proto_msg);

  const NestedPayload* it = &actual_proto;
  // Iterate all the way to the leaf node
  while (it->has_nested()) {
    it = &(it->nested());
  }
  return it->payload();
}

uint64_t GetStructProtoLayer(const std::string& proto_msg,
                             const std::string& field_name) {
  pb::Struct actual_proto;
  actual_proto.ParseFromString(proto_msg);

  uint64_t actual_layers = 0;
  const pb::Struct* it = &actual_proto;
  while (it->fields().contains(field_name)) {
    ++actual_layers;
    it = &(it->fields().at(field_name).struct_value());
  }
  return actual_layers;
}

// Parse the Grpc message binary to Json message using ResponseToJsonTranslator.
template <class ProtoMessageType>
std::string ParseGrpcMessageToJsonMessage(const ProtoMessageType& proto,
                                          absl::string_view msg_type,
                                          uint64_t num_checks, bool streaming,
                                          uint64_t stream_size) {
  std::string proto_binary;
  proto.SerializeToString(&proto_binary);
  std::string proto_binary_with_delimiter =
      WrapGrpcMessageWithDelimiter(proto_binary);

  if (streaming) {
    // Need to make a copy otherwise the call to absl::StrAppend is undefined.
    std::string copy = proto_binary_with_delimiter;
    // Append stream_size - 1 proto binary to the original binary string
    for (uint64_t i = 1; i < stream_size; ++i) {
      absl::StrAppend(&proto_binary_with_delimiter, copy);
    }
  }
  BenchmarkZeroCopyInputStream is(proto_binary_with_delimiter, num_checks);
  // We use newline delimited to get each JSON object separated by \n instead
  // of in an array.
  const JsonResponseTranslateOptions options{pb::util::JsonPrintOptions(),
                                             true};
  ResponseToJsonTranslator translator(
      GetBenchmarkTypeHelper().Resolver(),
      absl::StrFormat("type.googleapis.com/%s", msg_type), streaming, &is,
      options);

  std::string message;
  while (translator.NextMessage(&message)) {
  }
  EXPECT_TRUE(translator.Status().ok());

  return message;
}

}  // namespace

//
// Start of JSON to GRPC integration benchmark tests
//
TEST(BenchmarkInputStreamTest, IntegrationWithJsonRequestTranslatorBytesProto) {
  // JSON message containing "Hello World!" encoded in base64 string.
  absl::string_view json_msg = R"({"payload":"SGVsbG8gV29ybGQh"})";
  absl::string_view expected_decoded_payload = "Hello World!";

  const std::string proto_str =
      ParseJsonMessageToProtoMessage(json_msg, "BytesPayload", 1);

  // Verification - decoded message should equal the encoded one.
  BytesPayload actual_proto;
  actual_proto.ParseFromString(proto_str);
  EXPECT_EQ(expected_decoded_payload, actual_proto.payload());
}

TEST(BenchmarkInputStreamTest, IntegrationWithJsonRequestTranslatorArrayProto) {
  IntegrationWithJsonRequestTranslatorArrayProtoHelper<Int32ArrayPayload>(
      "Int32ArrayPayload");
  IntegrationWithJsonRequestTranslatorArrayProtoHelper<DoubleArrayPayload>(
      "DoubleArrayPayload");
  IntegrationWithJsonRequestTranslatorArrayProtoHelper<StringArrayPayload>(
      "StringArrayPayload");
}

TEST(BenchmarkInputStreamTest,
     IntegrationWithJsonRequestTranslatorNestedProto) {
  absl::string_view nested_field_name = "nested";
  uint64_t num_nested_layer_input[] = {0, 1, 2, 4, 8, 16, 32};
  for (uint64_t num_nested_layer : num_nested_layer_input) {
    const std::string json_msg = GetNestedJsonString(
        num_nested_layer, nested_field_name, "payload", "Hello World!");
    const std::string proto_str =
        ParseJsonMessageToProtoMessage(json_msg, "NestedPayload", 1);

    EXPECT_EQ(GetNestedProtoLayer(proto_str), num_nested_layer);
  }
}

TEST(BenchmarkInputStreamTest,
     IntegrationWithJsonRequestTranslatorStructProto) {
  absl::string_view nested_field_name = "nested";
  uint64_t num_nested_layer_input[] = {0, 1, 2, 4, 8, 16, 32};
  for (uint64_t num_nested_layer : num_nested_layer_input) {
    const std::string json_msg = GetNestedJsonString(
        num_nested_layer, nested_field_name, "payload", "Hello World!");
    const std::string proto_str =
        ParseJsonMessageToProtoMessage(json_msg, "google.protobuf.Struct", 1);

    EXPECT_EQ(GetStructProtoLayer(proto_str, std::string(nested_field_name)),
              num_nested_layer);
  }
}

TEST(BenchmarkInputStreamTest,
     IntegrationWithJsonRequestTranslatorChunkMessage) {
  // JSON message containing "Hello World!"
  absl::string_view expected_payload = "Hello World!";
  const std::string json_msg =
      absl::StrFormat(R"({"payload":"%s"})", expected_payload);
  uint64_t num_checks_input[] = {1, 2, 4, 8};

  for (uint64_t num_checks : num_checks_input) {
    std::string proto_str =
        ParseJsonMessageToProtoMessage(json_msg, "StringPayload", num_checks);

    // Verification - decoded message should equal the encoded one.
    StringPayload actual_proto;
    actual_proto.ParseFromString(proto_str);
    EXPECT_EQ(expected_payload, actual_proto.payload());
  }
}

//
// Start of gRPC to JSON integration benchmark tests
//
TEST(BenchmarkInputStreamTest,
     IntegrationWithGrpcResponseTranslatorBytesProto) {
  // Proto message containing "Hello World!"
  BytesPayload proto;
  pb::TextFormat::ParseFromString(R"(payload : "Hello World!")", &proto);
  // "SGVsbG8gV29ybGQh" is the base64 encoded string of "Hello World!"
  const std::string expected_json_str = R"({"payload": "SGVsbG8gV29ybGQh"})";

  const std::string json_str = ParseGrpcMessageToJsonMessage<BytesPayload>(
      proto, "BytesPayload", 1, false, 1);

  EXPECT_EQ(nlohmann::json::parse(json_str),
            nlohmann::json::parse(expected_json_str));
}

TEST(BenchmarkInputStreamTest,
     IntegrationWithGrpcResponseTranslatorArrayProto) {
  // Int32
  Int32ArrayPayload int32_arr_payload;
  pb::TextFormat::ParseFromString(R"(payload : [0,0,0])", &int32_arr_payload);
  const std::string expected_int32_json_str = R"({"payload":[0,0,0]})";
  const std::string int32_arr_json_str =
      ParseGrpcMessageToJsonMessage<Int32ArrayPayload>(
          int32_arr_payload, "Int32ArrayPayload", 1, false, 1);
  EXPECT_EQ(nlohmann::json::parse(int32_arr_json_str),
            nlohmann::json::parse(expected_int32_json_str));

  // Double
  DoubleArrayPayload double_arr_payload;
  pb::TextFormat::ParseFromString(R"(payload : [0,0,0])", &double_arr_payload);
  const std::string expected_double_arr_json_str = R"({"payload":[0,0,0]})";
  const std::string double_arr_json_str =
      ParseGrpcMessageToJsonMessage<DoubleArrayPayload>(
          double_arr_payload, "DoubleArrayPayload", 1, false, 1);
  EXPECT_EQ(nlohmann::json::parse(double_arr_json_str),
            nlohmann::json::parse(expected_double_arr_json_str));

  // String
  StringArrayPayload string_arr_payload;
  pb::TextFormat::ParseFromString(R"(payload : ["0","0","0"])",
                                  &string_arr_payload);
  const std::string expected_string_arr_json_str =
      R"({"payload":["0","0","0"]})";
  const std::string string_arr_json_str =
      ParseGrpcMessageToJsonMessage<StringArrayPayload>(
          string_arr_payload, "StringArrayPayload", 1, false, 1);
  EXPECT_EQ(nlohmann::json::parse(string_arr_json_str),
            nlohmann::json::parse(expected_string_arr_json_str));
}

TEST(BenchmarkInputStreamTest,
     IntegrationWithGrpcResponseTranslatorNestedProtoFlat) {
  NestedPayload zero_nested;
  pb::TextFormat::ParseFromString(R"(payload : "Hello World!")", &zero_nested);
  const std::string expected_zero_nested_json_str =
      R"({"payload": "Hello World!"})";
  const std::string zero_nested_json_str =
      ParseGrpcMessageToJsonMessage<NestedPayload>(zero_nested, "NestedPayload",
                                                   1, false, 1);
  EXPECT_EQ(nlohmann::json::parse(zero_nested_json_str),
            nlohmann::json::parse(expected_zero_nested_json_str));
}

TEST(BenchmarkInputStreamTest,
     IntegrationWithGrpcResponseTranslatorNestedProtoNested) {
  NestedPayload two_nested;
  pb::TextFormat::ParseFromString(
      R"(
      nested {
        nested {
          payload : "Hello World!"
        }
      })",
      &two_nested);
  const std::string expected_two_nested_json_str =
      R"({"nested": {"nested": {"payload": "Hello World!"}}})";
  const std::string two_nested_json_str =
      ParseGrpcMessageToJsonMessage<NestedPayload>(two_nested, "NestedPayload",
                                                   1, false, 1);
  EXPECT_EQ(nlohmann::json::parse(two_nested_json_str),
            nlohmann::json::parse(expected_two_nested_json_str));
}

TEST(BenchmarkInputStreamTest,
     IntegrationWithGrpcResponseTranslatorStructProtoFlat) {
  pb::Struct zero_nested;
  pb::TextFormat::ParseFromString(
      R"(
      fields {
        key: "payload"
        value { string_value: "Hello World!" }
      })",
      &zero_nested);
  const std::string expected_zero_nested_json_str =
      R"({"payload": "Hello World!"})";
  const std::string zero_nested_json_str =
      ParseGrpcMessageToJsonMessage<pb::Struct>(
          zero_nested, "google.protobuf.Struct", 1, false, 1);

  EXPECT_EQ(nlohmann::json::parse(zero_nested_json_str),
            nlohmann::json::parse(expected_zero_nested_json_str));
}

TEST(BenchmarkInputStreamTest,
     IntegrationWithGrpcResponseTranslatorStructProtoNested) {
  pb::Struct two_nested;
  pb::TextFormat::ParseFromString(
      R"(
        fields {
          key: "nested"
          value {
            struct_value: {
              fields {
                key: "nested"
                value {
                  struct_value: {
                    fields {
                      key: "payload"
                      value { string_value: "Hello World!" }
                    }
                  }
                }
              }
            }
          }
        })",
      &two_nested);
  const std::string expected_two_nested_json_str =
      R"({"nested": {"nested": {"payload": "Hello World!"}}})";
  const std::string two_nested_json_str =
      ParseGrpcMessageToJsonMessage<pb::Struct>(
          two_nested, "google.protobuf.Struct", 1, false, 1);

  EXPECT_EQ(nlohmann::json::parse(two_nested_json_str),
            nlohmann::json::parse(expected_two_nested_json_str));
}

TEST(BenchmarkInputStreamTest, IntegrationWithGrpcRequestTranslatorStreaming) {
  // JSON message containing "Hello World!"
  absl::string_view expected_payload = "Hello World!";
  const std::string json_msg =
      absl::StrFormat(R"({"payload":"%s"})", expected_payload);

  // Proto message containing "Hello World!"
  StringPayload proto;
  pb::TextFormat::ParseFromString(R"(payload : "Hello World!")", &proto);
  const std::string expected_json_str = R"({"payload": "Hello World!"})";

  for (uint64_t stream_size : {1, 2, 4, 8}) {
    const std::string json_str = ParseGrpcMessageToJsonMessage<StringPayload>(
        proto, "StringPayload", stream_size, true, stream_size);

    // Verification - decoded message should equal the encoded one.
    EXPECT_EQ(nlohmann::json::parse(json_str),
              nlohmann::json::parse(expected_json_str));
  }
}

TEST(BenchmarkInputStreamTest,
     IntegrationWithJsonRequestTranslatorNestedVariableBinding) {
  absl::string_view nested_field_name = "nested";
  uint64_t num_nested_layer_input[] = {0, 1, 2, 4, 8, 16, 32};
  for (uint64_t num_nested_layer : num_nested_layer_input) {
    // Variable value comes from binding, so an empty string is fine.
    const std::string json_msg = "{}";

    // Build the field_path bindings.
    // First, build the dot delimited binding string based on the number of
    // layers
    std::string field_path_str;
    for (uint64_t i = 0; i < num_nested_layer; ++i) {
      // Append the nested field name and a dot delimiter for each layer
      absl::StrAppend(&field_path_str, nested_field_name, ".");
    }
    // Append the actual payload field name
    absl::StrAppend(&field_path_str, "payload");

    // Second, parse the field_path object from the string
    auto field_path = ParseFieldPath(GetBenchmarkTypeHelper(), "NestedPayload",
                                     field_path_str);

    // Finally, construct the RequestInfo object containing the binding.
    // We only need to fill in variable_bindings, other fields are filled in
    // by BenchmarkJsonTranslation().
    RequestInfo request_info;
    request_info.variable_bindings = {
        RequestWeaver::BindingInfo{field_path, "Hello World!"}};

    const std::string proto_str = ParseJsonMessageToProtoMessage(
        json_msg, "NestedPayload", 1, request_info);

    EXPECT_EQ(GetNestedProtoLayer(proto_str), num_nested_layer);
    EXPECT_EQ(GetNestedProtoValue(proto_str), "Hello World!");
  }
}

}  // namespace perf_benchmark

}  // namespace transcoding
}  // namespace grpc
}  // namespace google