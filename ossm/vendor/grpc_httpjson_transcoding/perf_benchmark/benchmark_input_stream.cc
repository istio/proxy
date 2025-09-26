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
#include "absl/log/absl_check.h"

namespace google {
namespace grpc {
namespace transcoding {

namespace perf_benchmark {
BenchmarkZeroCopyInputStream::BenchmarkZeroCopyInputStream(
    std::string json_data, uint64_t num_checks)
    : finished_(false),
      msg_(std::move(json_data)),
      chunk_size_(msg_.size() / num_checks),
      pos_(0) {
  ABSL_CHECK(num_checks <= msg_.size());
}

int64_t BenchmarkZeroCopyInputStream::BytesAvailable() const {
  if (finished_) {
    return 0;
  }
  // check if we are at the last chunk
  if (pos_ + chunk_size_ >= msg_.size()) {
    return msg_.size() - pos_;
  }
  return chunk_size_;
}

bool BenchmarkZeroCopyInputStream::Next(const void** data, int* size) {
  if (finished_) {
    *size = 0;
    return false;
  }

  *data = msg_.data() + pos_;
  if (pos_ + chunk_size_ >= msg_.size()) {  // last message to be sent
    *size = msg_.size() - pos_;
  } else {
    *size = chunk_size_;
  }
  pos_ += *size;

  // Check if we have reached the end.
  if (pos_ >= msg_.size()) {
    finished_ = true;
  }
  return true;
}

void BenchmarkZeroCopyInputStream::Reset() {
  finished_ = false;
  pos_ = 0;
}

uint64_t BenchmarkZeroCopyInputStream::TotalBytes() const {
  return msg_.size();
}

void BenchmarkZeroCopyInputStream::BackUp(int count) {
  ABSL_CHECK(count <= pos_);
  pos_ -= count;
  finished_ = false;
}

int64_t BenchmarkZeroCopyInputStream::ByteCount() const { return pos_; }
}  // namespace perf_benchmark

}  // namespace transcoding
}  // namespace grpc
}  // namespace google
