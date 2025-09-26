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
#ifndef PERF_BENCHMARK_BENCHMARK_INPUT_STREAM_H_
#define PERF_BENCHMARK_BENCHMARK_INPUT_STREAM_H_

#include "absl/strings/string_view.h"
#include "absl/log/absl_log.h"
#include "grpc_transcoding/transcoder_input_stream.h"

namespace google {
namespace grpc {
namespace transcoding {

namespace perf_benchmark {
// An implementation of ZeroCopyInputStream for benchmarking.
// Subclasses of this should store the entire input message and return pointer
// to the stored message for each round of Next(). This is useful during
// benchmark since the same input message will be read multiple times without
// introducing a large runtime overhead.
//
// For streaming JSON messages, pass in an array of JSON objects as the msg.
//
// After each benchmark iteration, Reset() needs to be called.
class BenchmarkZeroCopyInputStream : public TranscoderInputStream {
 public:
  // Pre-Conditions:
  // - num_checks <= json_data.size()
  //
  // json_data - a std::string containing the JSON data to be read.
  // num_chunks - controls the number of calls to Next() that would yield the
  //              full JSON message.
  // Note: the actual number of checks could be off by a few chunks due to int
  // rounding.
  explicit BenchmarkZeroCopyInputStream(std::string json_data,
                                        uint64_t num_checks);
  ~BenchmarkZeroCopyInputStream() override = default;

  int64_t BytesAvailable() const override;
  bool Finished() const override { return finished_; };

  bool Next(const void** data, int* size) override;
  void BackUp(int count) override;
  bool Skip(int count) override {
    ABSL_LOG(FATAL) << "Not implemented";
    return false;
  };
  int64_t ByteCount() const override;

  // Reset the input stream back to the original start state.
  // This should be called after one iteration of benchmark.
  virtual void Reset();

  // Return the total number of bytes of the entire JSON message.
  virtual uint64_t TotalBytes() const;

 private:
  bool finished_;
  const std::string msg_;
  const uint64_t chunk_size_;
  uint64_t pos_;
};

}  // namespace perf_benchmark

}  // namespace transcoding
}  // namespace grpc
}  // namespace google

#endif  // PERF_BENCHMARK_BENCHMARK_INPUT_STREAM_H_
