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
#include "gtest/gtest.h"

namespace google {
namespace grpc {
namespace transcoding {

namespace perf_benchmark {
TEST(BenchmarkInputStreamTest, BenchmarkZeroCopyInputStreamSimple) {
  absl::string_view json_msg_input[] = {
      R"({"Hello":"World!"})",
      R"([{"Hello":"World!"}])",
      R"([{"Hello":"World!"},{"Hello":"World, Again!"}])"};

  for (auto& json_msg : json_msg_input) {
    BenchmarkZeroCopyInputStream is(std::string(json_msg), 1);

    // TotalBytes and BytesAvailable should equal to json_msg.
    EXPECT_EQ(is.TotalBytes(), json_msg.size());
    EXPECT_EQ(is.BytesAvailable(), json_msg.size());
    // Stream should not be finished.
    EXPECT_FALSE(is.Finished());

    // Reading data.
    const void* data = nullptr;
    int size;
    is.Next(&data, &size);
    EXPECT_EQ(size, json_msg.size());
    EXPECT_EQ(std::string(static_cast<const char*>(data)), json_msg);

    // Stream should be finished
    EXPECT_TRUE(is.Finished());

    // Reset should reset everything as if Next() is not called.
    is.Reset();
    EXPECT_EQ(is.TotalBytes(), json_msg.size());
    EXPECT_EQ(is.BytesAvailable(), json_msg.size());
    EXPECT_FALSE(is.Finished());
  }
}

TEST(BenchmarkInputStreamTest, BenchmarkZeroCopyInputStreamChunk) {
  absl::string_view json_msg = R"({"Hello":"World!"})";
  const uint64_t num_checks_input[] = {1, 2, 4, json_msg.size() - 1,
                                       json_msg.size()};

  for (uint64_t num_checks : num_checks_input) {
    BenchmarkZeroCopyInputStream is(std::string(json_msg), num_checks);
    uint64_t expected_chunk_size = json_msg.size() / num_checks;

    // Reading data.
    const void* data = nullptr;
    int size;
    uint64_t total_bytes_read = 0;
    std::string str_read;
    while (!is.Finished()) {
      // TotalBytes should equal to json_msg.
      EXPECT_EQ(is.TotalBytes(), json_msg.size());
      if (json_msg.size() - total_bytes_read >= expected_chunk_size) {
        // BytesAvailable should equal to the chunk size unless we are reading
        // the last message.
        EXPECT_EQ(is.BytesAvailable(), expected_chunk_size);
      }

      is.Next(&data, &size);
      total_bytes_read += size;
      str_read += std::string(static_cast<const char*>(data), size);

      if (json_msg.size() - total_bytes_read >= expected_chunk_size) {
        // size should equal to the expected_chunk_size unless it's the last
        // message.
        EXPECT_EQ(size, expected_chunk_size);
      }
      if (total_bytes_read == json_msg.size()) {
        // Stream should be finished
        EXPECT_TRUE(is.Finished());
      }
    }
    EXPECT_EQ(total_bytes_read, json_msg.size());
    EXPECT_EQ(str_read, json_msg);

    // Reset should reset everything as if Next() is not called.
    is.Reset();
    EXPECT_EQ(is.TotalBytes(), json_msg.size());
    EXPECT_EQ(is.BytesAvailable(), expected_chunk_size);
    EXPECT_FALSE(is.Finished());
  }
}

}  // namespace perf_benchmark

}  // namespace transcoding
}  // namespace grpc
}  // namespace google
