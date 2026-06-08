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
#include <google/protobuf/util/json_util.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "external/skywalking_data_collect_protocol/language-agent/Tracing.pb.h"
#include "mocks.h"
#include "source/propagation_impl.h"
#include "source/tracing_context_impl.h"

using google::protobuf::util::JsonStringToMessage;
using testing::NiceMock;
using testing::Return;

namespace cpp2sky {

static constexpr absl::string_view sample_ctx =
    "1-MQ==-NQ==-3-bWVzaA==-aW5zdGFuY2U=-L2FwaS92MS9oZWFsdGg=-"
    "ZXhhbXBsZS5jb206ODA4MA==";

class TracingContextTest : public testing::Test {
 public:
  TracingContextTest() {
    config_.set_service_name(service_name_);
    config_.set_instance_name(instance_name_);

    span_ctx_ = std::make_shared<SpanContextImpl>(sample_ctx);
    span_ext_ctx_ = std::make_shared<SpanContextExtensionImpl>("1");

    factory_.reset(new TracingContextFactory(config_));
  }

 protected:
  NiceMock<MockRandomGenerator> random_;
  std::string service_name_ = "mesh";
  std::string instance_name_ = "service_0";
  TracerConfig config_;
  SpanContextSharedPtr span_ctx_;
  SpanContextExtensionSharedPtr span_ext_ctx_;
  std::unique_ptr<TracingContextFactory> factory_;
};

TEST_F(TracingContextTest, BasicTest) {
  auto sc = factory_->create();
  EXPECT_EQ(sc->service(), "mesh");
  EXPECT_EQ(sc->serviceInstance(), "service_0");

  // No parent span
  auto span = sc->createEntrySpan();
  EXPECT_EQ(sc->spans().size(), 1);
  EXPECT_EQ(span->spanId(), 0);

  auto t1 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(100)));
  auto t2 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(200)));

  span->startSpan("sample1", t1);
  span->setPeer("localhost:9000");
  span->endSpan(t2);

  std::string json = R"EOF(
  {
    "spanId": "0",
    "parentSpanId": "-1",
    "startTime": "100",
    "endTime": "200",
    "peer": "localhost:9000",
    "spanType": "Entry",
    "spanLayer": "Http",
    "componentId": "9000",
    "operationName": "sample1",
    "skipAnalysis": "false",
  }
  )EOF";
  skywalking::v3::SpanObject expected_obj;
  JsonStringToMessage(json, &expected_obj);
  EXPECT_EQ(expected_obj.DebugString(), span->createSpanObject().DebugString());

  // With parent span
  auto span_child = sc->createExitSpan(std::move(span));
  EXPECT_EQ(sc->spans().size(), 2);
  EXPECT_EQ(span_child->spanId(), 1);

  t1 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(100)));
  t2 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(200)));

  span_child->startSpan("sample1", t1);
  span_child->setPeer("localhost:9000");
  span_child->endSpan(t2);

  std::string json2 = R"EOF(
  {
    "spanId": "1",
    "parentSpanId": "0",
    "startTime": "100",
    "endTime": "200",
    "peer": "localhost:9000",
    "spanType": "Exit",
    "spanLayer": "Http",
    "componentId": "9000",
    "operationName": "sample1",
    "skipAnalysis": "false",
  }
  )EOF";
  skywalking::v3::SpanObject expected_obj2;
  JsonStringToMessage(json2, &expected_obj2);
  EXPECT_EQ(expected_obj2.DebugString(),
            span_child->createSpanObject().DebugString());
}

TEST_F(TracingContextTest, ChildSegmentContext) {
  auto sc = factory_->create(span_ctx_);
  EXPECT_EQ(sc->service(), "mesh");
  EXPECT_EQ(sc->serviceInstance(), "service_0");

  // No parent span
  auto span = sc->createEntrySpan();
  EXPECT_EQ(sc->spans().size(), 1);
  EXPECT_EQ(span->spanId(), 0);

  auto t1 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(100)));
  auto t2 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(200)));

  span->startSpan("sample1", t1);
  span->setPeer("localhost:9000");
  span->setOperationName("sample11");
  span->endSpan(t2);

  std::string json = R"EOF(
  {
    "spanId": "0",
    "parentSpanId": "-1",
    "startTime": "100",
    "endTime": "200",
    "refs": {
      "refType": "CrossProcess",
      "traceId": "1",
      "parentTraceSegmentId": "5",
      "parentSpanId": 3,
      "parentService": "mesh",
      "parentServiceInstance": "instance",
      "parentEndpoint": "/api/v1/health",
      "networkAddressUsedAtPeer": "example.com:8080"
    },
    "peer": "localhost:9000",
    "spanType": "Entry",
    "spanLayer": "Http",
    "componentId": "9000",
    "skipAnalysis": "false",
    "operationName": "sample11",
  }
  )EOF";
  skywalking::v3::SpanObject expected_obj;
  JsonStringToMessage(json, &expected_obj);
  EXPECT_EQ(expected_obj.DebugString(), span->createSpanObject().DebugString());

  // With parent span
  auto span_child = sc->createExitSpan(std::move(span));
  EXPECT_EQ(sc->spans().size(), 2);
  EXPECT_EQ(span_child->spanId(), 1);

  t1 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(100)));
  t2 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(200)));

  span_child->startSpan("sample1", t1);

  span_child->setPeer("localhost:9000");
  span_child->addTag("category", "database");

  absl::string_view key = "method";
  absl::string_view value = "GETxxxx";
  value.remove_suffix(4);
  span_child->addTag(key, value);

  std::string log_key = "service_0";
  std::string log_value = "error";
  auto t3 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(300)));
  span_child->addLog(log_key, log_value, t3);

  absl::string_view log_key2 = "service_1";
  absl::string_view log_value2 = "succeeded\x01\x03";
  log_value2.remove_suffix(2);

  auto t4 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(300)));
  span_child->addLog(log_key2, log_value2, t4);

  span_child->endSpan(t2);

  std::string json2 = R"EOF(
  {
    "spanId": "1",
    "parentSpanId": "0",
    "startTime": "100",
    "endTime": "200",
    "peer": "localhost:9000",
    "spanType": "Exit",
    "spanLayer": "Http",
    "componentId": "9000",
    "skipAnalysis": "false",
    "tags": [
      {
        "key": "category",
        "value": "database"
      },
      {
        "key": "method",
        "value": "GET"
      }
    ],
    "logs": [
      {
        "time": "300",
        "data": {
          "key": "service_0",
          "value": "error"
        }
      },
      {
        "time": "300",
        "data": {
          "key": "service_1",
          "value": "succeeded"
        }
      }
    ],
    "operationName": "sample1",
  }
  )EOF";
  skywalking::v3::SpanObject expected_obj2;
  JsonStringToMessage(json2, &expected_obj2);
  EXPECT_EQ(expected_obj2.DebugString(),
            span_child->createSpanObject().DebugString());
}

TEST_F(TracingContextTest, SkipAnalysisSegment) {
  auto sc = factory_->create(span_ctx_, span_ext_ctx_);
  EXPECT_TRUE(sc->skipAnalysis());

  // No parent span
  auto span = sc->createEntrySpan();
  EXPECT_EQ(sc->spans().size(), 1);
  EXPECT_EQ(span->spanId(), 0);

  auto t1 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(100)));
  auto t2 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(200)));

  span->startSpan("sample1", t1);
  span->setPeer("localhost:9000");
  span->endSpan(t2);

  std::string json = R"EOF(
  {
    "spanId": "0",
    "parentSpanId": "-1",
    "startTime": "100",
    "endTime": "200",
    "peer": "localhost:9000",
    "spanType": "Entry",
    "spanLayer": "Http",
    "componentId": "9000",
    "operationName": "sample1",
    "skipAnalysis": "true",
    "refs": {
      "refType": "CrossProcess",
      "traceId": "1",
      "parentTraceSegmentId": "5",
      "parentSpanId": 3,
      "parentService": "mesh",
      "parentServiceInstance": "instance",
      "parentEndpoint": "/api/v1/health",
      "networkAddressUsedAtPeer": "example.com:8080"
    }
  }
  )EOF";
  skywalking::v3::SpanObject expected_obj;
  JsonStringToMessage(json, &expected_obj);
  EXPECT_EQ(expected_obj.DebugString(), span->createSpanObject().DebugString());
}

TEST_F(TracingContextTest, SW8CreateTest) {
  TracingContextImpl sc(config_.service_name(), config_.instance_name(),
                        span_ctx_, span_ext_ctx_, random_);
  EXPECT_EQ(sc.service(), "mesh");
  EXPECT_EQ(sc.serviceInstance(), "service_0");

  auto span = sc.createEntrySpan();
  EXPECT_EQ(sc.spans().size(), 1);
  EXPECT_EQ(span->spanId(), 0);
  span->startSpan("sample1");
  span->endSpan();

  std::string target_address("10.0.0.1:443");

  // Entry span should be rejected as propagation context
  EXPECT_FALSE(sc.createSW8HeaderValue(target_address).has_value());

  auto span2 = sc.createExitSpan(span);

  EXPECT_EQ(sc.spans().size(), 2);
  EXPECT_EQ(span2->spanId(), 1);
  span2->startSpan("sample2");
  span2->endSpan();

  std::string expect_sw8(
      "1-MQ==-dXVpZA==-1-bWVzaA==-c2VydmljZV8w-c2FtcGxlMQ==-"
      "MTAuMC4wLjE6NDQz");

  EXPECT_EQ(expect_sw8, *sc.createSW8HeaderValue(target_address));

  std::vector<char> target_address_based_vector;
  target_address_based_vector.reserve(target_address.size() * 2);

  target_address_based_vector = {'1', '0', '.', '0', '.', '0',
                                 '.', '1', ':', '4', '4', '3'};

  absl::string_view target_address_based_vector_view{
      target_address_based_vector.data(), target_address_based_vector.size()};

  EXPECT_EQ(target_address_based_vector.size(), target_address.size());
  EXPECT_EQ(expect_sw8,
            *sc.createSW8HeaderValue(target_address_based_vector_view));

  // Make sure that the end of target_address_based_vector_view is not '\0'. We
  // reserve enough memory for target_address_based_vector, so push back will
  // not cause content to be re-allocated.
  target_address_based_vector.push_back('x');
  EXPECT_EQ(expect_sw8,
            *sc.createSW8HeaderValue(target_address_based_vector_view));
}

TEST_F(TracingContextTest, ReadyToSendTest) {
  auto sc = factory_->create();

  // No parent span
  auto span = sc->createEntrySpan();
  EXPECT_EQ(sc->spans().size(), 1);
  EXPECT_EQ(span->spanId(), 0);

  auto t1 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(100)));
  auto t2 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(200)));

  span->startSpan("sample1", t1);
  span->setPeer("localhost:9000");
  span->endSpan(t2);

  EXPECT_TRUE(sc->readyToSend());

  auto span2 = sc->createExitSpan(span);
  auto t3 = TimePoint<SystemTime>(
      SystemTime(std::chrono::duration<int, std::milli>(300)));

  span->startSpan("sample1", t3);

  EXPECT_FALSE(sc->readyToSend());
}

TEST_F(TracingContextTest, TraceLogTest) {
  TracingContextImpl sc(config_.service_name(), config_.instance_name(),
                        span_ctx_, span_ext_ctx_, random_);
  EXPECT_EQ(
      "test\", \"SW_CTX\": [\"mesh\",\"service_0\",\"1\",\"uuid\",\"-1\"]}",
      sc.logMessage("test"));
}

}  // namespace cpp2sky
