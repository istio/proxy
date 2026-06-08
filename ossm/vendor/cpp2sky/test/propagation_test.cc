// Copyright 2020 SkyAPM

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/strings/string_view.h"
#include "cpp2sky/exception.h"
#include "source/propagation_impl.h"

namespace cpp2sky {

static constexpr absl::string_view sample =
    "1-MQ==-NQ==-3-bWVzaA==-aW5zdGFuY2U=-L2FwaS92MS9oZWFsdGg=-"
    "ZXhhbXBsZS5jb206ODA4MA==";

static constexpr absl::string_view less_field =
    "1-MQ==-NQ==-3-bWVzaA==-aW5zdGFuY2U=-L2FwaS92MS9oZWFsdGg=";

static constexpr absl::string_view more_field =
    "1-MQ==-NQ==-3-bWVzaA==-aW5zdGFuY2U=-L2FwaS92MS9oZWFsdGg=-"
    "ZXhhbXBsZS5jb206ODA4MA==-hogehoge";

static constexpr absl::string_view invalid_sample =
    "3-MQ==-NQ==-3-bWVzaA==-aW5zdGFuY2U=-L2FwaS92MS9oZWFsdGg=-"
    "ZXhhbXBsZS5jb206ODA4MA==";

static constexpr absl::string_view invalid_span_id =
    "1-MQ==-NQ==-abc-bWVzaA==-aW5zdGFuY2U=-L2FwaS92MS9oZWFsdGg=-"
    "ZXhhbXBsZS5jb206ODA4MA==";

TEST(TestSpanContext, Basic) {
  auto data = std::string(sample.data());
  SpanContextImpl sc(data);
  EXPECT_TRUE(sc.sample());
  EXPECT_EQ(sc.traceId(), "1");
  EXPECT_EQ(sc.traceSegmentId(), "5");
  EXPECT_EQ(sc.spanId(), 3);
  EXPECT_EQ(sc.service(), "mesh");
  EXPECT_EQ(sc.serviceInstance(), "instance");
  EXPECT_EQ(sc.endpoint(), "/api/v1/health");
  EXPECT_EQ(sc.targetAddress(), "example.com:8080");
}

TEST(TestSpanContext, MalformedSpanContext) {
  {
    auto data = std::string(less_field.data());
    EXPECT_THROW(SpanContextImpl{data}, TracerException);
  }
  {
    auto data = std::string(more_field.data());
    EXPECT_THROW(SpanContextImpl{data}, TracerException);
  }
  {
    auto data = std::string(invalid_sample.data());
    EXPECT_THROW(SpanContextImpl{data}, TracerException);
  }
  { EXPECT_THROW(SpanContextImpl{invalid_span_id}, TracerException); }
}

}  // namespace cpp2sky
