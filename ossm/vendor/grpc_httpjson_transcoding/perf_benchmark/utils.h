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
#ifndef PERF_BENCHMARK_UTILS_H_
#define PERF_BENCHMARK_UTILS_H_

#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "google/api/service.pb.h"
#include "google/protobuf/util/converter/type_info.h"
#include "perf_benchmark/benchmark.pb.h"
#include "src/include/grpc_transcoding/type_helper.h"

namespace google {
namespace grpc {
namespace transcoding {

namespace perf_benchmark {

// Load service from a proto text file. Returns true if loading succeeds;
// otherwise returns false.
absl::Status LoadService(absl::string_view config_pb_txt_file,
                         absl::string_view benchmark_path,
                         ::google::api::Service* service);
absl::Status LoadService(absl::string_view config_pb_txt_file,
                         ::google::api::Service* service);

// Return the given percentile of the vector v.
double GetPercentile(const std::vector<double>& v, double perc);

// This method is not thread-safe since it uses a shared absl::BitGen.
// Return a random string of the given length.
// length - Length of the returned string. If base64 == true, the actual
//          returned string length is 33–37% larger due to the encoding.
// base64 - True if the returned string should be base64 encoded. This is
//          required for bytes proto message.
std::string GetRandomBytesString(uint64_t length, bool base64);

// This method is not thread-safe since it uses a shared absl::BitGen.
// Return a random alphanumeric string of the given length.
// length - Length of the returned string.
std::string GetRandomAlphanumericString(uint64_t length);

// This method is not thread-safe since it uses a shared absl::BitGen.
// Return a random string representing an array of int32, e.g. "[1,2,3]"
// length - Length of the integer array.
std::string GetRandomInt32ArrayString(uint64_t length);

// Return an array string of the given length with repeated values,
// e.g. "[0, 0, 0]" for GetRepeatedValueArrayString("0", 3).
// val - Unescaped string value to be put in the array.
// length - Length of the integer array.
std::string GetRepeatedValueArrayString(absl::string_view val, uint64_t length);

// Return a nested JSON string with the innermost value being a payload string,
// e.g. "{"nested": {"nested": {"inner_key": "inner_val"}}}"
// layers - Number of nested layer. The value needs >= 0. 0 is a flat JSON.
// nested_field_name - JSON key name for the nested field.
// inner_key - Field name for the innermost json field.
// payload_msg - String value for the innermost json field.
std::string GetNestedJsonString(uint64_t layers,
                                absl::string_view nested_field_name,
                                absl::string_view inner_key,
                                absl::string_view inner_val);

// Return an HTTP/JSON string that corresponds to gRPC streaming message.
// This is essentially wrapping the json_msg repetitively around a JSON array.
// for stream_size == 1 -> "[json_msg]"
// for stream_size > 1 -> "[json_msg,...,json_msg]"
std::string GetStreamedJson(absl::string_view json_msg, uint64_t stream_size);

// Prefix the binary with a size to delimiter data segment and return.
std::string WrapGrpcMessageWithDelimiter(absl::string_view proto_binary);

// Return a unique_ptr to a NestedPayload object having the given `layers`.
std::unique_ptr<NestedPayload> GetNestedPayload(uint64_t layers,
                                                absl::string_view inner_val);

// Return a unique_ptr to a ::google::protobuf::Struct object.
std::unique_ptr<::google::protobuf::Struct> GetNestedStructPayload(
    uint64_t layers, absl::string_view nested_field_name,
    absl::string_view inner_key, absl::string_view inner_val);

// Parse a dot delimited field path string into a vector of actual field
// pointers.
std::vector<const google::protobuf::Field*> ParseFieldPath(
    const TypeHelper& type_helper, absl::string_view msg_type,
    const std::string& field_path_str);

// Generate a JSON string corresponds to MultiStringFieldMessage.
// For the 8 fields in the message, we will fill in the first
// `num_fields_exist`
// number of fields with the given `val`.
std::string GenerateMultiStringFieldPayloadJsonStr(
    uint64_t num_fields_exist, absl::string_view field_prefix,
    absl::string_view val);

}  // namespace perf_benchmark

}  // namespace transcoding
}  // namespace grpc
}  // namespace google

// Macros

// Macro for running a benchmark with p25, p75, p90, p99, p999 percentiles.
// Other statistics - mean, median, standard deviation, coefficient of variation
// are automatically captured.
// Note that running with 1000 iterations only gives 1 data point. Therefore,
// it is recommended to run with --benchmark_repetitions=1000 CLI argument to
// get comparable results.
// Use this marco the same way as BENCHMARK macro.
#define BENCHMARK_WITH_PERCENTILE(func)                                        \
  BENCHMARK(func)                                                              \
      ->ComputeStatistics("p25",                                               \
                          [](const std::vector<double>& v) -> double {         \
                            return GetPercentile(v, 25);                       \
                          })                                                   \
      ->ComputeStatistics("p75",                                               \
                          [](const std::vector<double>& v) -> double {         \
                            return GetPercentile(v, 75);                       \
                          })                                                   \
      ->ComputeStatistics("p90",                                               \
                          [](const std::vector<double>& v) -> double {         \
                            return GetPercentile(v, 90);                       \
                          })                                                   \
      ->ComputeStatistics("p99",                                               \
                          [](const std::vector<double>& v) -> double {         \
                            return GetPercentile(v, 99);                       \
                          })                                                   \
      ->ComputeStatistics("p999", [](const std::vector<double>& v) -> double { \
        return GetPercentile(v, 99.9);                                         \
      })

#define BENCHMARK_STREAMING_WITH_PERCENTILE(func) \
  BENCHMARK_WITH_PERCENTILE(func)->Arg(1)->Arg(1 << 2)->Arg(1 << 4)->Arg(1 << 6)

#endif  // PERF_BENCHMARK_UTILS_H_
