// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/tools/quic_memory_cache_backend.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/tools/quic_backend_response.h"
#include "quiche/common/platform/api/quiche_file_utils.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quic {
namespace test {

namespace {
using Response = QuicBackendResponse;

class TestRequestHandler : public QuicSimpleServerBackend::RequestHandler {
 public:
  ~TestRequestHandler() override = default;

  QuicConnectionId connection_id() const override { return QuicConnectionId(); }
  QuicStreamId stream_id() const override { return QuicStreamId(0); }
  std::string peer_host() const override { return "test.example.com"; }
  QuicSpdyStream* GetStream() override { return nullptr; }
  virtual void OnResponseBackendComplete(
      const QuicBackendResponse* response) override {
    response_headers_ = response->headers().Clone();
    response_body_ = response->body();
  }
  void SendStreamData(absl::string_view, bool) override {}
  void TerminateStreamWithError(QuicResetStreamError) override {}

  const quiche::HttpHeaderBlock& ResponseHeaders() const {
    return response_headers_;
  }
  const std::string& ResponseBody() const { return response_body_; }

 private:
  quiche::HttpHeaderBlock response_headers_;
  std::string response_body_;
};

}  // namespace

class QuicMemoryCacheBackendTest : public QuicTest {
 protected:
  void CreateRequest(std::string host, std::string path,
                     quiche::HttpHeaderBlock* headers) {
    (*headers)[":method"] = "GET";
    (*headers)[":path"] = path;
    (*headers)[":authority"] = host;
    (*headers)[":scheme"] = "https";
  }

  std::string CacheDirectory() {
    return quiche::test::QuicheGetTestMemoryCachePath();
  }

  QuicMemoryCacheBackend cache_;
};

TEST_F(QuicMemoryCacheBackendTest, GetResponseNoMatch) {
  const Response* response =
      cache_.GetResponse("mail.google.com", "/index.html");
  ASSERT_FALSE(response);
}

TEST_F(QuicMemoryCacheBackendTest, AddSimpleResponseGetResponse) {
  std::string response_body("hello response");
  cache_.AddSimpleResponse("www.google.com", "/", 200, response_body);

  quiche::HttpHeaderBlock request_headers;
  CreateRequest("www.google.com", "/", &request_headers);
  const Response* response = cache_.GetResponse("www.google.com", "/");
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers().contains(":status"));
  EXPECT_EQ("200", response->headers().find(":status")->second);
  EXPECT_EQ(response_body.size(), response->body().length());
}

TEST_F(QuicMemoryCacheBackendTest, AddResponse) {
  const std::string kRequestHost = "www.foo.com";
  const std::string kRequestPath = "/";
  const std::string kResponseBody("hello response");

  quiche::HttpHeaderBlock response_headers;
  response_headers[":status"] = "200";
  response_headers["content-length"] = absl::StrCat(kResponseBody.size());

  quiche::HttpHeaderBlock response_trailers;
  response_trailers["key-1"] = "value-1";
  response_trailers["key-2"] = "value-2";
  response_trailers["key-3"] = "value-3";

  cache_.AddResponse(kRequestHost, "/", response_headers.Clone(), kResponseBody,
                     response_trailers.Clone());

  const Response* response = cache_.GetResponse(kRequestHost, kRequestPath);
  EXPECT_EQ(response->headers(), response_headers);
  EXPECT_EQ(response->body(), kResponseBody);
  EXPECT_EQ(response->trailers(), response_trailers);
}

// TODO(crbug.com/1249712) This test is failing on iOS.
#if defined(OS_IOS)
#define MAYBE_ReadsCacheDir DISABLED_ReadsCacheDir
#else
#define MAYBE_ReadsCacheDir ReadsCacheDir
#endif
TEST_F(QuicMemoryCacheBackendTest, MAYBE_ReadsCacheDir) {
  cache_.InitializeBackend(CacheDirectory());
  const Response* response =
      cache_.GetResponse("test.example.com", "/index.html");
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers().contains(":status"));
  EXPECT_EQ("200", response->headers().find(":status")->second);
  // Connection headers are not valid in HTTP/2.
  EXPECT_FALSE(response->headers().contains("connection"));
  EXPECT_LT(0U, response->body().length());
}

// TODO(crbug.com/1249712) This test is failing on iOS.
#if defined(OS_IOS)
#define MAYBE_UsesOriginalUrl DISABLED_UsesOriginalUrl
#else
#define MAYBE_UsesOriginalUrl UsesOriginalUrl
#endif
TEST_F(QuicMemoryCacheBackendTest, MAYBE_UsesOriginalUrl) {
  cache_.InitializeBackend(CacheDirectory());
  const Response* response =
      cache_.GetResponse("test.example.com", "/site_map.html");
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers().contains(":status"));
  EXPECT_EQ("200", response->headers().find(":status")->second);
  // Connection headers are not valid in HTTP/2.
  EXPECT_FALSE(response->headers().contains("connection"));
  EXPECT_LT(0U, response->body().length());
}

// TODO(crbug.com/1249712) This test is failing on iOS.
#if defined(OS_IOS)
#define MAYBE_UsesOriginalUrlOnly DISABLED_UsesOriginalUrlOnly
#else
#define MAYBE_UsesOriginalUrlOnly UsesOriginalUrlOnly
#endif
TEST_F(QuicMemoryCacheBackendTest, MAYBE_UsesOriginalUrlOnly) {
  // Tests that if the URL cannot be inferred correctly from the path
  // because the directory does not include the hostname, that the
  // X-Original-Url header's value will be used.
  std::string dir;
  std::string path = "map.html";
  std::vector<std::string> files;
  ASSERT_TRUE(quiche::EnumerateDirectoryRecursively(CacheDirectory(), files));
  for (const std::string& file : files) {
    if (absl::EndsWithIgnoreCase(file, "map.html")) {
      dir = file;
      dir.erase(dir.length() - path.length() - 1);
      break;
    }
  }
  ASSERT_NE("", dir);

  cache_.InitializeBackend(dir);
  const Response* response =
      cache_.GetResponse("test.example.com", "/site_map.html");
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers().contains(":status"));
  EXPECT_EQ("200", response->headers().find(":status")->second);
  // Connection headers are not valid in HTTP/2.
  EXPECT_FALSE(response->headers().contains("connection"));
  EXPECT_LT(0U, response->body().length());
}

TEST_F(QuicMemoryCacheBackendTest, DefaultResponse) {
  // Verify GetResponse returns nullptr when no default is set.
  const Response* response = cache_.GetResponse("www.google.com", "/");
  ASSERT_FALSE(response);

  // Add a default response.
  quiche::HttpHeaderBlock response_headers;
  response_headers[":status"] = "200";
  response_headers["content-length"] = "0";
  Response* default_response = new Response;
  default_response->set_headers(std::move(response_headers));
  cache_.AddDefaultResponse(default_response);

  // Now we should get the default response for the original request.
  response = cache_.GetResponse("www.google.com", "/");
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers().contains(":status"));
  EXPECT_EQ("200", response->headers().find(":status")->second);

  // Now add a set response for / and make sure it is returned
  cache_.AddSimpleResponse("www.google.com", "/", 302, "");
  response = cache_.GetResponse("www.google.com", "/");
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers().contains(":status"));
  EXPECT_EQ("302", response->headers().find(":status")->second);

  // We should get the default response for other requests.
  response = cache_.GetResponse("www.google.com", "/asd");
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->headers().contains(":status"));
  EXPECT_EQ("200", response->headers().find(":status")->second);
}

TEST_F(QuicMemoryCacheBackendTest, Echo) {
  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "POST";
  request_headers[":path"] = "/echo";
  const std::string request_body("hello request");
  TestRequestHandler handler;
  cache_.FetchResponseFromBackend(request_headers, request_body, &handler);
  EXPECT_EQ("200", handler.ResponseHeaders().find(":status")->second);
  EXPECT_EQ(request_body, handler.ResponseBody());  // Echoed back.
}

}  // namespace test
}  // namespace quic
