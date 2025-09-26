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
#include "benchmark/benchmark.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/log/absl_check.h"
#include "google/api/service.pb.h"
#include "google/protobuf/text_format.h"
#include "grpc_transcoding/json_request_translator.h"
#include "grpc_transcoding/request_message_translator.h"
#include "grpc_transcoding/response_to_json_translator.h"
#include "grpc_transcoding/type_helper.h"

#include "absl/random/random.h"
#include "perf_benchmark/benchmark.pb.h"
#include "perf_benchmark/benchmark_input_stream.h"
#include "perf_benchmark/utils.h"

namespace google {
namespace grpc {
namespace transcoding {

namespace perf_benchmark {
namespace {
using namespace benchmark;
namespace pb = ::google::protobuf;

constexpr absl::string_view kServiceConfigTextProtoFile =
    "benchmark_service.textproto";
constexpr absl::string_view kBytesPayloadMessageType = "BytesPayload";
constexpr absl::string_view kStringPayloadMessageType = "StringPayload";
constexpr absl::string_view kNestedPayloadMessageType = "NestedPayload";
constexpr absl::string_view kInt32ArrayPayloadMessageType = "Int32ArrayPayload";
constexpr absl::string_view kStructPayloadMessageType =
    "google.protobuf.Struct";
constexpr absl::string_view kDoubleArrayPayloadMessageType =
    "DoubleArrayPayload";
constexpr absl::string_view kStringArrayPayloadMessageType =
    "StringArrayPayload";
constexpr absl::string_view kMultiStringFieldPayloadMessageType =
    "MultiStringFieldPayload";

// Used for NestedPayload and StructPayload.
// It has to be 31 because gRPC to JSON transcoding has a limit of 32 layers.
constexpr uint64_t kNumNestedLayersForStreaming = 31;
constexpr absl::string_view kNestedFieldName = "nested";
constexpr absl::string_view kInnerMostNestedFieldName = "payload";
constexpr absl::string_view kInnerMostNestedFieldValue = "Deep Hello World!";
// Used for ArrayPayload
constexpr uint64_t kArrayPayloadLength = 1 << 10;  // 1024
// Used for BytesPayload
constexpr uint64_t kBytesPayloadLengthForStreaming = 1 << 20;  // 1 MiB
// Used for Int32ArrayPayload
constexpr uint64_t kInt32ArrayPayloadLengthForStreaming = 1 << 14;  // 16384
// Used for Segmented StringPayload
constexpr uint64_t kSegmentedStringPayloadLength = 1 << 20;            // 1 MiB
constexpr uint64_t kSegmentedStringStreamingNumChunksPerMsg = 1 << 8;  // 256
// Used for MultiStringFieldPayload
constexpr uint64_t kNumFieldsInMultiStringFieldPayload = 8;
constexpr absl::string_view kMultiStringFieldPrefix = "f";

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
}  // namespace

// Helper function to check status. It will call state.SkipWithError if the
// status is not OK.
void SkipWithErrorIfNotOk(::benchmark::State& state,
                          const absl::Status& status) {
  if (!status.ok()) {
    state.SkipWithError(status.ToString().c_str());
  }
}

// Helper function to add custom benchmark counters to the state object.
void AddBenchmarkCounters(::benchmark::State& state, uint64_t num_message,
                          uint64_t total_bytes) {
  auto request_processed = static_cast<double>(state.iterations());
  auto message_processed =
      static_cast<double>(state.iterations() * num_message);
  auto bytes_processed = static_cast<double>(state.iterations() * total_bytes);
  state.counters["byte_throughput"] =
      Counter(bytes_processed, Counter::kIsRate, Counter::kIs1024);
  state.counters["byte_latency"] = Counter(
      bytes_processed, Counter::kIsRate | Counter::kInvert, Counter::kIs1024);
  state.counters["request_throughput"] =
      Counter(request_processed, Counter::kIsRate);
  state.counters["request_latency"] =
      Counter(request_processed, Counter::kIsRate | Counter::kInvert);
  state.counters["message_throughput"] =
      Counter(message_processed, Counter::kIsRate);
  state.counters["message_latency"] =
      Counter(message_processed, Counter::kIsRate | Counter::kInvert);
}

// Helper function to run Json Translation benchmark.
//
// state - ::benchmark::State& variable used for collecting metrics.
// msg_type - Protobuf message name for translation.
// json_msg - Complete input json message.
// streaming - Flag for streaming testing. When true, a stream of `stream_size`
//             number of `json_msg` will be fed into translation.
// stream_size - Number of streaming messages.
// num_checks - Number of calls to NextMessage() that yields the full message.
// request_info - RequestInfo object specifies the URI mapping and bindings.
//                body_field_path and message_type field of the object will be
//                filled in from this method.
absl::Status BenchmarkJsonTranslation(::benchmark::State& state,
                                      absl::string_view msg_type,
                                      absl::string_view json_msg,
                                      bool streaming, uint64_t stream_size,
                                      uint64_t num_checks,
                                      RequestInfo request_info = {}) {
  // Retrieve global type helper
  const TypeHelper& type_helper = GetBenchmarkTypeHelper();

  // Get message type
  const pb::Type* type = type_helper.Info()->GetTypeByTypeUrl(
      absl::StrFormat("type.googleapis.com/%s", msg_type));
  if (nullptr == type) {
    return absl::InvalidArgumentError(
        absl::StrCat("Could not resolve the message type ", msg_type));
  }

  // body field path used in this benchmark are all "*"
  request_info.body_field_path = "*";
  request_info.message_type = type;

  // Wrap json_msg inside BenchmarkZeroCopyInputStream.
  std::unique_ptr<BenchmarkZeroCopyInputStream> is;
  if (streaming) {
    std::string streaming_msg = GetStreamedJson(json_msg, stream_size);
    is = absl::make_unique<BenchmarkZeroCopyInputStream>(streaming_msg,
                                                         num_checks);
  } else {
    is = absl::make_unique<BenchmarkZeroCopyInputStream>(std::string(json_msg),
                                                         num_checks);
  }

  // Benchmark the transcoding process
  std::string message;
  for (auto s : state) {
    JsonRequestTranslator translator(type_helper.Resolver(), is.get(),
                                     request_info, streaming, false);
    MessageStream& out = translator.Output();

    if (!out.Status().ok()) {
      return absl::InternalError(out.Status().ToString());
    }

    while (out.NextMessage(&message)) {
    }
    is->Reset();  // low overhead.
  }

  // Add custom benchmark counters.
  AddBenchmarkCounters(state, streaming ? stream_size : 1, is->TotalBytes());

  return absl::OkStatus();
}

// Helper function to run gRPC Translation benchmark.
// We use newline_delimited == true option which can generate JSON object in
// streaming translation when the full message is not sent.
//
// template ProtoMessageType - ProtoBuffer message object that will be
//                             serialized for translation.
// state - ::benchmark::State& variable used for collecting metrics.
// msg_type - Protobuf message name for translation.
// proto - ProtoBuffer object with ProtoMessageType
// streaming - Flag for streaming testing. When true, a stream of `stream_size`
//             number of `json_msg` will be fed into translation.
// stream_size - Number of streaming messages.
// num_checks - Number of calls to NextMessage() that yields the full message.
template <class ProtoMessageType>
absl::Status BenchmarkGrpcTranslation(::benchmark::State& state,
                                      absl::string_view msg_type,
                                      const ProtoMessageType& proto,
                                      bool streaming, uint64_t stream_size,
                                      uint64_t num_checks) {
  std::string proto_binary;
  proto.SerializeToString(&proto_binary);
  std::string proto_binary_with_delimiter =
      WrapGrpcMessageWithDelimiter(proto_binary);

  if (streaming) {
    // Combine stream_size proto binary to the original binary string
    std::stringstream ss("");
    for (uint64_t i = 0; i < stream_size; ++i) {
      ss << proto_binary_with_delimiter;
    }
    proto_binary_with_delimiter = ss.str();
  }

  // Wrap proto binary inside BenchmarkZeroCopyInputStream.
  BenchmarkZeroCopyInputStream is(proto_binary_with_delimiter, num_checks);

  // Benchmark the transcoding process
  std::string message;
  const JsonResponseTranslateOptions options{pb::util::JsonPrintOptions(),
                                             true};
  for (auto s : state) {
    ResponseToJsonTranslator translator(
        GetBenchmarkTypeHelper().Resolver(),
        absl::StrFormat("type.googleapis.com/%s", msg_type), streaming, &is,
        options);

    while (translator.NextMessage(&message)) {
    }

    if (!translator.Status().ok()) {
      return absl::InternalError(translator.Status().ToString());
    }

    is.Reset();  // low overhead.
  }

  // Add custom benchmark counters.
  AddBenchmarkCounters(state, streaming ? stream_size : 1, is.TotalBytes());

  return absl::OkStatus();
}

// Helper function for benchmarking single bytes payload translation from JSON.
void SinglePayloadFromJson(::benchmark::State& state, uint64_t payload_length,
                           bool streaming, uint64_t stream_size) {
  std::string json_msg = absl::StrFormat(
      R"({"payload" : "%s"})", GetRandomBytesString(payload_length, true));

  auto status = BenchmarkJsonTranslation(state, kBytesPayloadMessageType,
                                         json_msg, streaming, stream_size, 1);
  SkipWithErrorIfNotOk(state, status);
}

// Helper function for benchmarking single bytes payload translation from gRPC.
void SinglePayloadFromGrpc(::benchmark::State& state, uint64_t payload_length,
                           bool streaming, uint64_t stream_size) {
  BytesPayload proto;
  proto.set_payload(GetRandomBytesString(payload_length, true));

  auto status = BenchmarkGrpcTranslation<BytesPayload>(
      state, kBytesPayloadMessageType, proto, streaming, stream_size, 1);
  SkipWithErrorIfNotOk(state, status);
}

static void BM_SinglePayloadFromJsonNonStreaming(::benchmark::State& state) {
  SinglePayloadFromJson(state, state.range(0), false, 0);
}

static void BM_SinglePayloadFromJsonStreaming(::benchmark::State& state) {
  SinglePayloadFromJson(state, kBytesPayloadLengthForStreaming, true,
                        state.range(0));
}

static void BM_SinglePayloadFromGrpcNonStreaming(::benchmark::State& state) {
  SinglePayloadFromGrpc(state, state.range(0), false, 0);
}

static void BM_SinglePayloadFromGrpcStreaming(::benchmark::State& state) {
  SinglePayloadFromGrpc(state, kBytesPayloadLengthForStreaming, true,
                        state.range(0));
}

// Helper function for benchmarking int32 array payload translation from JSON.
void Int32ArrayPayloadFromJson(::benchmark::State& state, uint64_t array_length,
                               bool streaming, uint64_t stream_size) {
  std::string json_msg = absl::StrFormat(
      R"({"payload" : %s})", GetRandomInt32ArrayString(array_length));

  auto status = BenchmarkJsonTranslation(state, kInt32ArrayPayloadMessageType,
                                         json_msg, streaming, stream_size, 1);
  SkipWithErrorIfNotOk(state, status);
}

// Helper function for benchmarking int32 array payload translation from gRPC.
void Int32ArrayPayloadFromGrpc(::benchmark::State& state, uint64_t array_length,
                               bool streaming, uint64_t stream_size) {
  static absl::BitGen bitgen;
  Int32ArrayPayload proto;
  for (int i = 0; i < array_length; ++i) {
    proto.add_payload(absl::Uniform(bitgen, std::numeric_limits<int32_t>::min(),
                                    std::numeric_limits<int32_t>::max()));
  }

  auto status = BenchmarkGrpcTranslation<Int32ArrayPayload>(
      state, kBytesPayloadMessageType, proto, streaming, stream_size, 1);
  SkipWithErrorIfNotOk(state, status);
}

static void BM_Int32ArrayPayloadFromJsonNonStreaming(
    ::benchmark::State& state) {
  Int32ArrayPayloadFromJson(state, state.range(0), false, 0);
}

static void BM_Int32ArrayPayloadFromJsonStreaming(::benchmark::State& state) {
  Int32ArrayPayloadFromJson(state, kInt32ArrayPayloadLengthForStreaming, true,
                            state.range(0));
}

static void BM_Int32ArrayPayloadFromGrpcNonStreaming(
    ::benchmark::State& state) {
  Int32ArrayPayloadFromGrpc(state, state.range(0), false, 0);
}

static void BM_Int32ArrayPayloadFromGrpcStreaming(::benchmark::State& state) {
  Int32ArrayPayloadFromGrpc(state, kInt32ArrayPayloadLengthForStreaming, true,
                            state.range(0));
}

// Helper function for benchmarking translation from JSON to payload of
// different types.
void ArrayPayloadFromJson(::benchmark::State& state, absl::string_view msg_type,
                          bool streaming, uint64_t stream_size) {
  auto json_msg = absl::StrFormat(
      R"({"payload" : %s})",
      GetRepeatedValueArrayString("0", kArrayPayloadLength));

  auto status = BenchmarkJsonTranslation(state, msg_type, json_msg, streaming,
                                         stream_size, 1);
  SkipWithErrorIfNotOk(state, status);
}

// Helper function for benchmarking translation from gRPC to payload of
// different types.
// template ProtoMessageType - ProtoBuffer message type. E.g. this can be
//                             BytesPayload.
// template PayloadType - `payload` field type of the ProtoBuffer message.
//                        E.g. the `payload` field in `BytesPayload` is bytes,
//                        which corresponds to std::string in C++. Then,
//                        PayloadType for BytesPayload ProtoMessageType is
//                        std::string.
template <class ProtoMessageType, class PayloadType>
void ArrayPayloadFromGrpc(::benchmark::State& state, absl::string_view msg_type,
                          const PayloadType& val, bool streaming,
                          uint64_t stream_size) {
  ProtoMessageType proto;
  for (uint64_t i = 0; i < kArrayPayloadLength; ++i) {
    proto.add_payload(val);
  }

  auto status = BenchmarkGrpcTranslation<ProtoMessageType>(
      state, msg_type, proto, streaming, stream_size, 1);
  SkipWithErrorIfNotOk(state, status);
}

static void BM_Int32ArrayTypePayloadFromJsonNonStreaming(
    ::benchmark::State& state) {
  ArrayPayloadFromJson(state, kInt32ArrayPayloadMessageType, false, 0);
}
static void BM_DoubleArrayTypePayloadFromJsonNonStreaming(
    ::benchmark::State& state) {
  ArrayPayloadFromJson(state, kDoubleArrayPayloadMessageType, false, 0);
}
static void BM_StringArrayTypePayloadFromJsonNonStreaming(
    ::benchmark::State& state) {
  ArrayPayloadFromJson(state, kStringArrayPayloadMessageType, false, 0);
}

static void BM_Int32ArrayTypePayloadFromGrpcNonStreaming(
    ::benchmark::State& state) {
  ArrayPayloadFromGrpc<Int32ArrayPayload, int32_t>(
      state, kInt32ArrayPayloadMessageType, 0, false, 0);
}
static void BM_DoubleArrayTypePayloadFromGrpcNonStreaming(
    ::benchmark::State& state) {
  ArrayPayloadFromGrpc<DoubleArrayPayload, double>(
      state, kDoubleArrayPayloadMessageType, 0.0, false, 0);
}
static void BM_StringArrayTypePayloadFromGrpcNonStreaming(
    ::benchmark::State& state) {
  ArrayPayloadFromGrpc<StringArrayPayload, std::string>(
      state, kStringArrayPayloadMessageType, "0", false, 0);
}

// Helper function for benchmarking translation from nested JSON values.
void NestedPayloadFromJson(::benchmark::State& state, uint64_t layers,
                           bool streaming, uint64_t stream_size,
                           absl::string_view msg_type) {
  const std::string json_msg = GetNestedJsonString(
      layers, kNestedFieldName, std::string(kInnerMostNestedFieldName),
      kInnerMostNestedFieldValue);

  auto status = BenchmarkJsonTranslation(state, msg_type, json_msg, streaming,
                                         stream_size, 1);
  SkipWithErrorIfNotOk(state, status);
}

// Helper function for benchmarking translation from nested gRPC values.
void NestedPayloadFromGrpc(::benchmark::State& state, uint64_t layers,
                           bool streaming, uint64_t stream_size,
                           absl::string_view msg_type) {
  std::unique_ptr<NestedPayload> proto =
      GetNestedPayload(layers, kInnerMostNestedFieldValue);

  auto status = BenchmarkGrpcTranslation<NestedPayload>(
      state, msg_type, *proto, streaming, stream_size, 1);
  SkipWithErrorIfNotOk(state, status);
}

// Helper function for benchmarking translation from nested gRPC values.
void StructPayloadFromGrpc(::benchmark::State& state, uint64_t layers,
                           bool streaming, uint64_t stream_size,
                           absl::string_view msg_type) {
  std::unique_ptr<pb::Struct> proto = GetNestedStructPayload(
      layers, std::string(kNestedFieldName),
      std::string(kInnerMostNestedFieldName), kInnerMostNestedFieldValue);

  auto status = BenchmarkGrpcTranslation<pb::Struct>(state, msg_type, *proto,
                                                     streaming, stream_size, 1);
  SkipWithErrorIfNotOk(state, status);
}

static void BM_NestedProtoPayloadFromJsonNonStreaming(
    ::benchmark::State& state) {
  NestedPayloadFromJson(state, state.range(0), false, 0,
                        kNestedPayloadMessageType);
}

static void BM_NestedProtoPayloadFromJsonStreaming(::benchmark::State& state) {
  NestedPayloadFromJson(state, kNumNestedLayersForStreaming, true,
                        state.range(0), kNestedPayloadMessageType);
}

static void BM_NestedProtoPayloadFromGrpcNonStreaming(
    ::benchmark::State& state) {
  NestedPayloadFromGrpc(state, state.range(0), false, 0,
                        kNestedPayloadMessageType);
}

static void BM_NestedProtoPayloadFromGrpcStreaming(::benchmark::State& state) {
  NestedPayloadFromGrpc(state, kNumNestedLayersForStreaming, true,
                        state.range(0), kNestedPayloadMessageType);
}

static void BM_StructProtoPayloadFromJsonNonStreaming(
    ::benchmark::State& state) {
  NestedPayloadFromJson(state, state.range(0), false, 0,
                        kStructPayloadMessageType);
}

static void BM_StructProtoPayloadFromJsonStreaming(::benchmark::State& state) {
  NestedPayloadFromJson(state, kNumNestedLayersForStreaming, true,
                        state.range(0), kStructPayloadMessageType);
}

static void BM_StructProtoPayloadFromGrpcNonStreaming(
    ::benchmark::State& state) {
  StructPayloadFromGrpc(state, state.range(0), false, 0,
                        kStructPayloadMessageType);
}

static void BM_StructProtoPayloadFromGrpcStreaming(::benchmark::State& state) {
  StructPayloadFromGrpc(state, kNumNestedLayersForStreaming, true,
                        state.range(0), kStructPayloadMessageType);
}

// Helper function for benchmarking translation from segmented JSON input
void SegmentedStringPayloadFromJson(::benchmark::State& state,
                                    uint64_t payload_length, bool streaming,
                                    uint64_t stream_size, uint64_t num_checks) {
  // We are using GetRandomAlphanumericString instead of GetRandomBytesString
  // because JSON format reserves characters such as `"` and `\`.
  // We could generate `"` and `\` and escape them, but for simplicity, we are
  // only using alphanumeric characters.
  // This would also be a more common for string proto.
  const std::string json_msg = absl::StrFormat(
      R"({"payload" : "%s"})", GetRandomAlphanumericString(payload_length));

  auto status =
      BenchmarkJsonTranslation(state, kStringPayloadMessageType, json_msg,
                               streaming, stream_size, num_checks);
  SkipWithErrorIfNotOk(state, status);
}

static void BM_SegmentedStringPayloadFromJsonNonStreaming(
    ::benchmark::State& state) {
  SegmentedStringPayloadFromJson(state, kSegmentedStringPayloadLength, false, 0,
                                 state.range(0));
}

static void BM_SegmentedStringPayloadFromJsonStreaming(
    ::benchmark::State& state) {
  // due to streaming, num_chunks_per_msg will be multiplied with the
  // stream_size
  uint64_t stream_size = state.range(0);
  uint64_t num_chunks_per_msg =
      kSegmentedStringStreamingNumChunksPerMsg * stream_size;
  SegmentedStringPayloadFromJson(state, kSegmentedStringPayloadLength, true,
                                 stream_size, num_chunks_per_msg);
}

// Helper function for benchmarking translation from nested JSON input with URI
// bindings.
void NestedVariableBindingsPayloadFromJson(::benchmark::State& state,
                                           uint64_t layers, bool streaming,
                                           uint64_t stream_size) {
  // This json string will also work if it's an empty JSON object since the
  // variable value comes from the binding. However, to better compare with
  // NestedPayloadFromJson() benchmarks to see the extra overhead of the
  // bindings, we use the same nested JSON string as in NestedPayloadFromJson().
  const std::string json_msg = GetNestedJsonString(
      layers, kNestedFieldName, std::string(kInnerMostNestedFieldName),
      kInnerMostNestedFieldValue);

  // Build the field_path bindings.
  // First, build the dot delimited binding string based on the number of layers
  std::string field_path_str;
  for (uint64_t i = 0; i < layers; ++i) {
    // Append the nested field name and a dot delimiter for each layer
    absl::StrAppend(&field_path_str, kNestedFieldName, ".");
  }
  // Append the actual payload field name
  absl::StrAppend(&field_path_str, kInnerMostNestedFieldName);

  // Second, parse the field_path object from the string
  auto field_path = ParseFieldPath(GetBenchmarkTypeHelper(),
                                   kNestedPayloadMessageType, field_path_str);

  // Finally, construct the RequestInfo object containing the binding.
  // We only need to fill in variable_bindings, other fields are filled in
  // by BenchmarkJsonTranslation().
  RequestInfo request_info;
  request_info.variable_bindings = {RequestWeaver::BindingInfo{
      field_path, std::string(kInnerMostNestedFieldValue)}};

  absl::Status status =
      BenchmarkJsonTranslation(state, kNestedPayloadMessageType, json_msg,
                               streaming, stream_size, 1, request_info);
  if (!status.ok()) {
    state.SkipWithError(status.ToString().c_str());
  }
}

static void BM_NestedVariableBindingsFromJsonNonStreaming(
    ::benchmark::State& state) {
  NestedVariableBindingsPayloadFromJson(state, state.range(0), false, 0);
}

// Helper function for benchmarking translation from JSON input with multiple
// URI bindings.
void NumVariableBindingsPayloadFromJson(::benchmark::State& state,
                                        uint64_t num_bound_vars, bool streaming,
                                        uint64_t stream_size) {
  if (num_bound_vars > kNumFieldsInMultiStringFieldPayload) {
    state.SkipWithError(
        "Number of free variables exceeds the total number of fields in "
        "MultiStringFieldMessage");
    return;
  }

  // 1 MiB of data
  const std::string field_val = GetRandomAlphanumericString(1 << 20);
  // Generate a JSON message that has all the fields filled in.
  // This is to make sure the input JSON size is equal for each benchmark, which
  // nicely separates out the variable bindings as a single control variable.
  const std::string json_msg = GenerateMultiStringFieldPayloadJsonStr(
      kNumFieldsInMultiStringFieldPayload, kMultiStringFieldPayloadMessageType,
      field_val);

  // Create variable bindings for the bound fields.
  // Note that field number starts at 1.
  RequestInfo request_info;
  for (uint64_t i = 1; i <= num_bound_vars; ++i) {
    auto field_path = ParseFieldPath(
        GetBenchmarkTypeHelper(), kMultiStringFieldPayloadMessageType,
        absl::StrFormat("%s%d", kMultiStringFieldPrefix, i));

    request_info.variable_bindings.emplace_back(
        RequestWeaver::BindingInfo{field_path, field_val});
  }

  absl::Status status = BenchmarkJsonTranslation(
      state, kMultiStringFieldPayloadMessageType, json_msg, streaming,
      stream_size, 1, request_info);
  if (!status.ok()) {
    state.SkipWithError(status.ToString().c_str());
  }
}

static void BM_NumVariableBindingsPayloadFromJsonNonStreaming(
    ::benchmark::State& state) {
  NumVariableBindingsPayloadFromJson(state, state.range(0), false, 0);
}

//
// Independent benchmark variable: JSON body length.
//
BENCHMARK_WITH_PERCENTILE(BM_SinglePayloadFromJsonNonStreaming)
    ->Arg(1)         // 1 byte
    ->Arg(1 << 10)   // 1 KiB
    ->Arg(1 << 20)   // 1 MiB
    ->Arg(1 << 25);  // 32 MiB
BENCHMARK_WITH_PERCENTILE(BM_SinglePayloadFromGrpcNonStreaming)
    ->Arg(1)         // 1 byte
    ->Arg(1 << 10)   // 1 KiB
    ->Arg(1 << 20)   // 1 MiB
    ->Arg(1 << 25);  // 32 MiB
BENCHMARK_STREAMING_WITH_PERCENTILE(BM_SinglePayloadFromJsonStreaming);
BENCHMARK_STREAMING_WITH_PERCENTILE(BM_SinglePayloadFromGrpcStreaming);

//
// Independent benchmark variable: JSON array length.
//
BENCHMARK_WITH_PERCENTILE(BM_Int32ArrayPayloadFromJsonNonStreaming)
    ->Arg(1)         // 1 val
    ->Arg(1 << 8)    // 256 vals
    ->Arg(1 << 10)   // 1024 vals
    ->Arg(1 << 14);  // 16384 vals
BENCHMARK_WITH_PERCENTILE(BM_Int32ArrayPayloadFromGrpcNonStreaming)
    ->Arg(1)         // 1 val
    ->Arg(1 << 8)    // 256 vals
    ->Arg(1 << 10)   // 1024 vals
    ->Arg(1 << 14);  // 16384 vals
BENCHMARK_STREAMING_WITH_PERCENTILE(BM_Int32ArrayPayloadFromJsonStreaming);
BENCHMARK_STREAMING_WITH_PERCENTILE(BM_Int32ArrayPayloadFromGrpcStreaming);

//
// Independent benchmark variable: JSON value data type.
// E.g. "0" can be parsed as int32, double, or string.
// Only non-streaming is benchmarked since the JSON is already an array.
// Benchmarks for array typed JSON streaming is tested with the JSON array
// length benchmark variable.
//
BENCHMARK_WITH_PERCENTILE(BM_Int32ArrayTypePayloadFromJsonNonStreaming);
BENCHMARK_WITH_PERCENTILE(BM_Int32ArrayTypePayloadFromGrpcNonStreaming);
BENCHMARK_WITH_PERCENTILE(BM_DoubleArrayTypePayloadFromJsonNonStreaming);
BENCHMARK_WITH_PERCENTILE(BM_DoubleArrayTypePayloadFromGrpcNonStreaming);
BENCHMARK_WITH_PERCENTILE(BM_StringArrayTypePayloadFromJsonNonStreaming);
BENCHMARK_WITH_PERCENTILE(BM_StringArrayTypePayloadFromGrpcNonStreaming);

//
// Independent benchmark variable: Number of nested JSON layer.
//
BENCHMARK_WITH_PERCENTILE(BM_NestedProtoPayloadFromJsonNonStreaming)
    ->Arg(0)    // flat JSON
    ->Arg(1)    // nested with 1 layer
    ->Arg(8)    // nested with 8 layers
                // More than 32 layers would fail the parsing for struct proto.
                // To be consistent for all nested cases, we set to 31.
    ->Arg(31);  // nested with 31 layers
BENCHMARK_WITH_PERCENTILE(BM_NestedProtoPayloadFromGrpcNonStreaming)
    ->Arg(0)    // flat JSON
    ->Arg(1)    // nested with 1 layer
    ->Arg(8)    // nested with 8 layers
                // More than 32 layers would fail the parsing for struct proto.
                // To be consistent for all nested cases, we set to 31.
    ->Arg(31);  // nested with 31 layers
BENCHMARK_WITH_PERCENTILE(BM_StructProtoPayloadFromJsonNonStreaming)
    ->Arg(0)    // flat JSON
    ->Arg(1)    // nested with 1 layer
    ->Arg(8)    // nested with 8 layers
                // More than 32 layers would fail the parsing for struct proto.
                // To be consistent for all nested cases, we set to 31.
    ->Arg(31);  // nested with 31 layers
BENCHMARK_WITH_PERCENTILE(BM_StructProtoPayloadFromGrpcNonStreaming)
    ->Arg(0)    // flat JSON
    ->Arg(1)    // nested with 1 layer
    ->Arg(8)    // nested with 8 layers
                // More than 32 layers would fail the parsing for struct proto.
                // To be consistent for all nested cases, we set to 31.
    ->Arg(31);  // nested with 31 layers
BENCHMARK_STREAMING_WITH_PERCENTILE(BM_NestedProtoPayloadFromJsonStreaming);
BENCHMARK_STREAMING_WITH_PERCENTILE(BM_NestedProtoPayloadFromGrpcStreaming);
BENCHMARK_STREAMING_WITH_PERCENTILE(BM_StructProtoPayloadFromJsonStreaming);
BENCHMARK_STREAMING_WITH_PERCENTILE(BM_StructProtoPayloadFromGrpcStreaming);

//
// Independent benchmark variable: Message chunk per message
// This only applies to JSON -> gRPC since gRPC -> JSON transcoding requires a
// complete message for the parsing, whereas incomplete JSON message can be
// stored in a buffer.
//
BENCHMARK_WITH_PERCENTILE(BM_SegmentedStringPayloadFromJsonNonStreaming)
    ->Arg(1)         // 1 chunk per message
    ->Arg(1 << 4)    // 16 chunks per message
    ->Arg(1 << 8)    // 256 chunks per message
    ->Arg(1 << 12);  // 4096 chunks per message
BENCHMARK_STREAMING_WITH_PERCENTILE(BM_SegmentedStringPayloadFromJsonStreaming);

//
// Independent benchmark variable: Variable binding depth.
// This only applies to JSON -> gRPC since there's no URI bindings from gRPC.
// Streaming benchmark doesn't apply here because the same insights can be
// collected from the Number of nested JSON layer benchmarks.
//
BENCHMARK_WITH_PERCENTILE(BM_NestedVariableBindingsFromJsonNonStreaming)
    ->Arg(0)    // flat JSON
    ->Arg(1)    // nested with 1 layer
    ->Arg(8)    // nested with 8 layers
    ->Arg(31);  // nested with 31 layers

//
// Independent benchmark variable: Number of variable bindings.
// This only applies to JSON -> gRPC since there's no URI bindings from gRPC.
// Streaming benchmark doesn't apply here because the same insights can be
// collected from the JSON body length benchmarks.
//
BENCHMARK_WITH_PERCENTILE(BM_NumVariableBindingsPayloadFromJsonNonStreaming)
    ->Arg(0)   // 0 bound variable
    ->Arg(2)   // 2 bound variables
    ->Arg(4)   // 4 bound variables
    ->Arg(8);  // 8 bound variables

// Benchmark Main function
BENCHMARK_MAIN();

}  // namespace perf_benchmark

}  // namespace transcoding
}  // namespace grpc
}  // namespace google
