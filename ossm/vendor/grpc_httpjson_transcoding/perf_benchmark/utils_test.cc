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
#include "perf_benchmark/utils.h"
#include <memory>
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_split.h"
#include "gtest/gtest.h"

namespace google {
namespace grpc {
namespace transcoding {

namespace perf_benchmark {

TEST(UtilsTest, GetRandomBytesStringLength) {
  const int test_length_input[] = {0, 1, 10, 100};
  for (auto length : test_length_input) {
    // Regular random string
    EXPECT_EQ(GetRandomBytesString(length, false).length(), length);

    // Base64 encoded random string should have the given length after decoding
    std::string decoded;
    absl::Base64Unescape(GetRandomBytesString(length, true), &decoded);
    EXPECT_EQ(decoded.length(), length);
  }
}

TEST(UtilsTest, GetPercentile) {
  // Fill in an array of 0 to 99
  std::vector<double> arr;
  int arr_length = 100;
  for (int i = 0; i < arr_length; ++i) {
    arr.push_back(double(i));
  }

  // i^th percentile should equal to i
  for (int i = 0; i < arr.size(); ++i) {
    EXPECT_EQ(GetPercentile(arr, double(i)), double(i));
  }

  // p999 should get the largest value
  EXPECT_EQ(GetPercentile(arr, 99.9), 99.0);
}

TEST(UtilsTest, GetRandomAlphanumericString) {
  for (auto ch : GetRandomAlphanumericString(100)) {
    std::cout << ch << std::endl;
    EXPECT_TRUE(absl::ascii_isalnum(ch));
  }
}

TEST(UtilsTest, GetRandomAlphanumericStringLength) {
  const int test_length_input[] = {0, 1, 10, 100};
  for (auto length : test_length_input) {
    EXPECT_EQ(GetRandomAlphanumericString(length).length(), length);
  }
}

TEST(UtilsTest, GetRandomInt32ArrayString) {
  const int test_length_input[] = {0, 1, 10, 100};
  for (auto length : test_length_input) {
    std::string res = GetRandomInt32ArrayString(length);
    EXPECT_EQ(res.front(), '[');
    EXPECT_EQ(res.back(), ']');

    // Verify length
    std::vector<std::string> split =
        absl::StrSplit(res.substr(1, res.size() - 2), ',');
    if (!split.empty() && split.at(0) != "") {  // if a delimiter is found
      EXPECT_EQ(split.size(), length);
    }
  }
}

TEST(UtilsTest, GetRepeatedValueArrayString) {
  const int test_length_input[] = {0, 1, 10, 100};
  absl::string_view test_val = "TEST";
  absl::string_view expected_json_val = R"("TEST")";
  for (auto length : test_length_input) {
    std::string res = GetRepeatedValueArrayString(test_val, length);
    EXPECT_EQ(res.front(), '[');
    EXPECT_EQ(res.back(), ']');

    // Verify length
    std::vector<std::string> split =
        absl::StrSplit(res.substr(1, res.size() - 2), ',');
    if (split.at(0) != "") {  // if a delimiter is found
      EXPECT_EQ(split.size(), length);
      for (const auto& s : split) {
        EXPECT_EQ(expected_json_val, s);
      }
    }
  }
}

TEST(UtilsTest, GetNestedJsonStringZeroLayer) {
  EXPECT_EQ(
      R"({"inner_val":"inner_key"})",
      GetNestedJsonString(0, "doesnt_matter", "inner_val", "inner_key"));
}

TEST(UtilsTest, GetNestedJsonStringMultiLayers) {
  EXPECT_EQ(
      R"({"nested_field_name":{"inner_val":"inner_key"}})",
      GetNestedJsonString(1, "nested_field_name", "inner_val", "inner_key"));
  EXPECT_EQ(
      R"({"nested_field_name":{"nested_field_name":{"inner_val":"inner_key"}}})",
      GetNestedJsonString(2, "nested_field_name", "inner_val", "inner_key"));
}

TEST(UtilsTest, GetNestedPayload) {
  std::string payload = "Hello World!";
  for (uint64_t num_layers : {0, 5, 50, 100}) {
    std::unique_ptr<NestedPayload> proto =
        GetNestedPayload(num_layers, payload);
    uint64_t counter = 0;
    const NestedPayload* it = proto.get();
    while (it->has_nested()) {
      ++counter;
      it = &it->nested();
    }
    EXPECT_EQ(it->payload(), payload);
    EXPECT_EQ(counter, num_layers);
  }
}

TEST(UtilsTest, GetNestedStructPayload) {
  std::string inner_val = "Hello World!";
  for (uint64_t num_layers : {0, 5, 50, 100}) {
    std::unique_ptr<::google::protobuf::Struct> proto =
        GetNestedStructPayload(num_layers, "nested", "payload", inner_val);
    uint64_t counter = 0;
    const ::google::protobuf::Struct* it = proto.get();
    while (it->fields().contains("nested")) {
      ++counter;
      it = &it->fields().at("nested").struct_value();
    }
    EXPECT_EQ(it->fields().at("payload").string_value(), inner_val);
    EXPECT_EQ(counter, num_layers);
  }
}

}  // namespace perf_benchmark

}  // namespace transcoding
}  // namespace grpc
}  // namespace google
