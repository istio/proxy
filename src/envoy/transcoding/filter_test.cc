/* Copyright 2017 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "src/envoy/transcoding/filter.h"

#include "common/buffer/buffer_impl.h"
#include "common/http/header_map_impl.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/printers.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;

using google::protobuf::util::Status;
using google::protobuf::util::error::Code;

namespace Grpc {
namespace Transcoding {

class MockTranscodingConfig : public Transcoding::Config {
 public:
  MockTranscodingConfig() {}
  ~MockTranscodingConfig() {}
  MOCK_METHOD5(CreateTranscoder,
               google::protobuf::util::Status(
                   const Http::HeaderMap& headers,
                   google::protobuf::io::ZeroCopyInputStream* request_input,
                   google::api_manager::transcoding::TranscoderInputStream* response_input,
                   std::unique_ptr<google::api_manager::transcoding::Transcoder>& transcoder,
                   const google::protobuf::MethodDescriptor*& method_descriptor));
};

class GrpcHttpJsonTranscodingFilterTest : public testing::Test {
 public:
  GrpcHttpJsonTranscodingFilterTest() : filter_(config_) {
    filter_.setDecoderFilterCallbacks(decoder_callbacks_);
    filter_.setEncoderFilterCallbacks(encoder_callbacks_);
  }

  NiceMock<MockTranscodingConfig> config_;
  Transcoding::Instance filter_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
};

TEST_F(GrpcHttpJsonTranscodingFilterTest, NoTranscoding) {
  ON_CALL(config_, CreateTranscoder(_, _, _, _, _)).WillByDefault(Return(Status(Code::NOT_FOUND, "")));

  Http::TestHeaderMapImpl request_headers{{"content-type", "application/grpc"},
                                          {":path", "/grpc.service/GrpcMethod"}};

  Http::TestHeaderMapImpl original_request_headers{{"content-type", "application/grpc"},
                                                   {":path", "/grpc.service/GrpcMethod"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_.decodeHeaders(request_headers, false));
  EXPECT_EQ(original_request_headers, request_headers);
}

}
}