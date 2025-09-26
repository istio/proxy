// Copyright 2021 SkyAPM

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
#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>

#include "cpp2sky/config.pb.h"
#include "cpp2sky/internal/async_client.h"
#include "mocks.h"
#include "source/tracer_impl.h"

namespace cpp2sky {

TEST(TracerTest, MatchedOpShouldIgnored) {
  TracerConfig config;
  *config.add_ignore_operation_name_suffix() = "/ignored";

  TracerImpl tracer(config, TraceAsyncClientPtr{
                                new testing::NiceMock<MockTraceAsyncClient>()});
  auto context = tracer.newContext();
  auto span = context->createEntrySpan();

  span->startSpan("/hoge/ignored");
  span->endSpan();

  EXPECT_FALSE(tracer.report(context));
}

TEST(TracerTest, NotClosedSpanExists) {
  TracerConfig config;

  TracerImpl tracer(config, TraceAsyncClientPtr{
                                new testing::NiceMock<MockTraceAsyncClient>()});
  auto context = tracer.newContext();
  auto span = context->createEntrySpan();

  span->startSpan("/hoge");

  EXPECT_FALSE(tracer.report(context));
}

TEST(TracerTest, Success) {
  TracerConfig config;

  auto mock_reporter = std::unique_ptr<MockTraceAsyncClient>{
      new testing::NiceMock<MockTraceAsyncClient>()};
  EXPECT_CALL(*mock_reporter, sendMessage(_));

  TracerImpl tracer(config, std::move(mock_reporter));
  auto context = tracer.newContext();
  auto span = context->createEntrySpan();

  span->startSpan("/hoge");
  span->endSpan();

  EXPECT_TRUE(tracer.report(context));
}

}  // namespace cpp2sky
