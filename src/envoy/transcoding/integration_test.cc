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

#include "common/grpc/codec.h"
#include "common/grpc/common.h"
#include "common/http/message_impl.h"
#include "src/envoy/transcoding/test/bookstore.pb.h"

#include "google/protobuf/stubs/status.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"

#include "test/integration/integration.h"
#include "test/mocks/http/mocks.h"

using google::protobuf::util::MessageDifferencer;
using google::protobuf::util::Status;
using google::protobuf::Message;

namespace Envoy {
namespace {

class TranscodingIntegrationTest : public BaseIntegrationTest,
                                   public testing::Test {
 public:
  /**
   * Global initializer for all integration tests.
   */
  void SetUp() override {
    fake_upstreams_.emplace_back(
        new FakeUpstream(0, FakeHttpConnection::Type::HTTP2));
    registerPort("upstream_0",
                 fake_upstreams_.back()->localAddress()->ip()->port());
    createTestServer("src/envoy/transcoding/test/integration.json", {"http"});
  }

  /**
   * Global destructor for all integration tests.
   */
  void TearDown() override {
    test_server_.reset();
    fake_upstreams_.clear();
  }

 protected:
  template <class RequestType, class ResponseType>
  void testTranscoding(Http::Message& request,
                       const std::vector<RequestType>& grpc_request_messages,
                       const std::vector<ResponseType>& grpc_response_messages,
                       const Status& grpc_status,
                       Http::Message& expected_response) {
    IntegrationCodecClientPtr codec_client;
    FakeHttpConnectionPtr fake_upstream_connection;
    IntegrationStreamDecoderPtr response(
        new IntegrationStreamDecoder(*dispatcher_));
    FakeStreamPtr request_stream;

    codec_client =
        makeHttpConnection(lookupPort("http"), Http::CodecClient::Type::HTTP1);

    if (request.body()) {
      Http::StreamEncoder& encoder =
          codec_client->startRequest(request.headers(), *response);
      codec_client->sendData(encoder, *request.body(), true);
    } else {
      codec_client->makeHeaderOnlyRequest(request.headers(), *response);
    }

    if (!grpc_request_messages.empty()) {
      fake_upstream_connection =
          fake_upstreams_[0]->waitForHttpConnection(*dispatcher_);
      request_stream = fake_upstream_connection->waitForNewStream();
      request_stream->waitForEndStream(*dispatcher_);

      Grpc::Decoder grpc_decoder;
      std::vector<Grpc::Frame> frames;
      grpc_decoder.decode(request_stream->body(), frames);

      EXPECT_EQ(grpc_request_messages.size(), frames.size());

      for (size_t i = 0; i < grpc_request_messages.size(); ++i) {
        RequestType actual_message;
        actual_message.ParseFromArray(
            frames[i].data_->linearize(frames[i].length_), frames[i].length_);

        EXPECT_TRUE(MessageDifferencer::Equals(grpc_request_messages[i],
                                               actual_message));
      }
    }

    if (request_stream) {
      Http::TestHeaderMapImpl response_headers;
      response_headers.insertStatus().value(200);
      response_headers.insertContentType().value(
          std::string("application/grpc"));
      if (grpc_response_messages.empty()) {
        response_headers.insertGrpcStatus().value(grpc_status.error_code());
        response_headers.insertGrpcMessage().value(grpc_status.error_message());
        request_stream->encodeHeaders(response_headers, true);
      } else {
        request_stream->encodeHeaders(response_headers, false);
        for (const auto& response_message : grpc_response_messages) {
          auto buffer = Grpc::Common::serializeBody(response_message);
          request_stream->encodeData(*buffer, false);
        }
        Http::TestHeaderMapImpl response_trailers;
        response_trailers.insertGrpcStatus().value(grpc_status.error_code());
        response_trailers.insertGrpcMessage().value(
            grpc_status.error_message());
        request_stream->encodeTrailers(response_trailers);
      }
      EXPECT_TRUE(request_stream->complete());
    }

    response->waitForEndStream();
    EXPECT_TRUE(response->complete());
    expected_response.headers().iterate(
        [](const Http::HeaderEntry& entry, void* context) -> void {
          IntegrationStreamDecoder* response =
              static_cast<IntegrationStreamDecoder*>(context);
          Http::LowerCaseString lower_key{entry.key().c_str()};
          EXPECT_STREQ(entry.value().c_str(),
                       response->headers().get(lower_key)->value().c_str());
        },
        response.get());
    if (expected_response.body()) {
      EXPECT_EQ(expected_response.bodyAsString(), response->body());
    }

    codec_client->close();
    fake_upstream_connection->close();
    fake_upstream_connection->waitForDisconnect();
  }
};

TEST_F(TranscodingIntegrationTest, BasicUnary) {
  Http::MessagePtr request(new Http::RequestMessageImpl(Http::HeaderMapPtr{
      new Http::TestHeaderMapImpl{{":method", "POST"},
                                  {":path", "/shelf"},
                                  {":authority", "host"},
                                  {"content-type", "application/json"}}}));
  request->body().reset(new Buffer::OwnedImpl{"{\"theme\": \"Children\"}"});
  bookstore::CreateShelfRequest csr;
  csr.mutable_shelf()->set_theme("Children");

  bookstore::Shelf response_pb;
  response_pb.set_id(20);
  response_pb.set_theme("Children");

  Http::MessagePtr response(new Http::ResponseMessageImpl(
      Http::HeaderMapPtr{new Http::TestHeaderMapImpl{
          {":status", "200"}, {"content-type", "application/json"}}}));

  response->body().reset(
      new Buffer::OwnedImpl{"{\"id\":\"20\",\"theme\":\"Children\"}"});

  testTranscoding<bookstore::CreateShelfRequest, bookstore::Shelf>(
      *request, {csr}, {response_pb}, Status::OK, *response);
}

}  // namespace
}  // namespace Envoy
