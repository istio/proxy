#include "quiche/http2/adapter/oghttp2_util.h"

#include <utility>
#include <vector>

#include "quiche/http2/adapter/http2_protocol.h"
#include "quiche/http2/adapter/test_frame_sequence.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace test {
namespace {

using HeaderPair = std::pair<absl::string_view, absl::string_view>;

TEST(ToHeaderBlock, EmptySpan) {
  quiche::HttpHeaderBlock block = ToHeaderBlock({});
  EXPECT_TRUE(block.empty());
}

TEST(ToHeaderBlock, ExampleRequestHeaders) {
  const std::vector<HeaderPair> pairs = {{":authority", "example.com"},
                                         {":method", "GET"},
                                         {":path", "/example.html"},
                                         {":scheme", "http"},
                                         {"accept", "text/plain, text/html"}};
  const std::vector<Header> headers = ToHeaders(pairs);
  quiche::HttpHeaderBlock block = ToHeaderBlock(headers);
  EXPECT_THAT(block, testing::ElementsAreArray(pairs));
}

TEST(ToHeaderBlock, ExampleResponseHeaders) {
  const std::vector<HeaderPair> pairs = {
      {":status", "403"},
      {"content-length", "1023"},
      {"x-extra-info", "humblest apologies"}};
  const std::vector<Header> headers = ToHeaders(pairs);
  quiche::HttpHeaderBlock block = ToHeaderBlock(headers);
  EXPECT_THAT(block, testing::ElementsAreArray(pairs));
}

TEST(ToHeaderBlock, RepeatedRequestHeaderNames) {
  const std::vector<HeaderPair> pairs = {
      {":authority", "example.com"},     {":method", "GET"},
      {":path", "/example.html"},        {":scheme", "http"},
      {"cookie", "chocolate_chips=yes"}, {"accept", "text/plain, text/html"},
      {"cookie", "raisins=no"}};
  const std::vector<HeaderPair> expected = {
      {":authority", "example.com"},
      {":method", "GET"},
      {":path", "/example.html"},
      {":scheme", "http"},
      {"cookie", "chocolate_chips=yes; raisins=no"},
      {"accept", "text/plain, text/html"}};
  const std::vector<Header> headers = ToHeaders(pairs);
  quiche::HttpHeaderBlock block = ToHeaderBlock(headers);
  EXPECT_THAT(block, testing::ElementsAreArray(expected));
}

TEST(ToHeaderBlock, RepeatedResponseHeaderNames) {
  const std::vector<HeaderPair> pairs = {
      {":status", "403"},          {"x-extra-info", "sorry"},
      {"content-length", "1023"},  {"x-extra-info", "humblest apologies"},
      {"content-length", "1024"},  {"set-cookie", "chocolate_chips=yes"},
      {"set-cookie", "raisins=no"}};
  const std::vector<HeaderPair> expected = {
      {":status", "403"},
      {"x-extra-info", absl::string_view("sorry\0humblest apologies", 24)},
      {"content-length", absl::string_view("1023"
                                           "\0"
                                           "1024",
                                           9)},
      {"set-cookie", absl::string_view("chocolate_chips=yes\0raisins=no", 30)}};
  const std::vector<Header> headers = ToHeaders(pairs);
  quiche::HttpHeaderBlock block = ToHeaderBlock(headers);
  EXPECT_THAT(block, testing::ElementsAreArray(expected));
}

}  // namespace
}  // namespace test
}  // namespace adapter
}  // namespace http2
