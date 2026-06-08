#include "quiche/binary_http/binary_http_message.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

using ::absl::StatusCode::kInternal;
using ::absl::StatusCode::kInvalidArgument;
using ::quiche::test::QuicheTestWithParam;
using ::testing::_;
using ::testing::ContainerEq;
using ::testing::FieldsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::TestParamInfo;
using ::testing::ValuesIn;

namespace quiche {
namespace {

std::string WordToBytes(uint32_t word) {
  return std::string({static_cast<char>(word >> 24),
                      static_cast<char>(word >> 16),
                      static_cast<char>(word >> 8), static_cast<char>(word)});
}

template <class T>
absl::Status TestPrintTo(const T& resp) {
  std::ostringstream os;
  PrintTo(resp, &os);
  if (os.str() != resp.DebugString()) {
    return absl::FailedPreconditionError(absl::StrCat(
        "PrintTo output to stream does not match DebugString: stream=",
        os.str(), ", DebugString=", resp.DebugString()));
  }
  return absl::OkStatus();
}

class RequestMessageSectionTestHandler
    : public BinaryHttpRequest::IndeterminateLengthDecoder::
          MessageSectionHandler {
 public:
  struct MessageData {
    std::optional<BinaryHttpRequest::ControlData> control_data_;
    std::vector<std::pair<std::string, std::string>> headers_;
    bool headers_done_ = false;
    std::vector<std::string> body_chunks_;
    bool body_chunks_done_ = false;
    std::vector<std::pair<std::string, std::string>> trailers_;
    bool trailers_done_ = false;
  };
  RequestMessageSectionTestHandler() = default;
  absl::Status OnControlData(
      const BinaryHttpRequest::ControlData& control_data) override {
    if (message_data_.control_data_.has_value()) {
      return absl::FailedPreconditionError(
          "OnControlData called multiple times");
    }
    message_data_.control_data_ = control_data;
    return absl::OkStatus();
  }
  absl::Status OnHeader(absl::string_view name,
                        absl::string_view value) override {
    if (message_data_.headers_done_) {
      return absl::FailedPreconditionError(
          "OnHeader called after OnHeadersDone");
    }
    message_data_.headers_.push_back({std::string(name), std::string(value)});
    return absl::OkStatus();
  }
  absl::Status OnHeadersDone() override {
    if (message_data_.headers_done_) {
      return absl::FailedPreconditionError(
          "OnHeadersDone called multiple times");
    }
    message_data_.headers_done_ = true;
    return absl::OkStatus();
  }
  absl::Status OnBodyChunk(absl::string_view body_chunk) override {
    if (message_data_.body_chunks_done_) {
      return absl::FailedPreconditionError(
          "OnBodyChunk called after OnBodyChunksDone");
    }
    message_data_.body_chunks_.push_back(std::string(body_chunk));
    return absl::OkStatus();
  }
  absl::Status OnBodyChunksDone() override {
    if (message_data_.body_chunks_done_) {
      return absl::FailedPreconditionError(
          "OnBodyChunksDone called multiple times");
    }
    message_data_.body_chunks_done_ = true;
    return absl::OkStatus();
  }
  absl::Status OnTrailer(absl::string_view name,
                         absl::string_view value) override {
    if (message_data_.trailers_done_) {
      return absl::FailedPreconditionError(
          "OnTrailer called after OnTrailersDone");
    }
    message_data_.trailers_.push_back({std::string(name), std::string(value)});
    return absl::OkStatus();
  }
  absl::Status OnTrailersDone() override {
    if (message_data_.trailers_done_) {
      return absl::FailedPreconditionError(
          "OnTrailersDone called multiple times");
    }
    message_data_.trailers_done_ = true;
    return absl::OkStatus();
  }
  MessageData& GetMessageData() { return message_data_; }

 private:
  MessageData message_data_;
};

constexpr absl::string_view kFramingIndicator =
    "02";  // 1-byte framing indicator
constexpr absl::string_view k2ByteFramingIndicator =
    "4002";  // 2-byte framing indicator
constexpr absl::string_view k8ByteContentTerminator =
    "C000000000000000";  // 8-byte content terminator
constexpr absl::string_view k4ByteContentTerminator =
    "80000000";  // 4-byte content terminator
constexpr absl::string_view kContentTerminator =
    "00";                                         // 1-byte content terminator
constexpr absl::string_view kPadding = "000000";  // 3-byte padding
constexpr absl::string_view kIndeterminateLengthEncodedRequestControlData =
    "04504F5354"              // :method = POST
    "056874747073"            // :scheme = https
    "0A676F6F676C652E636F6D"  // :authority = "google.com"
    "062F68656C6C6F";         // :path = /hello
constexpr absl::string_view kIndeterminateLengthEncodedRequestHeaders =
    "0A757365722D6167656E74"  // user-agent
    "346375726C2F372E31362E33206C69626375726C2F372E31362E33204F70656E53534C2F"
    "302E392E376C207A6C69622F312E322E33"  // curl/7.16.3 libcurl/7.16.3
                                          // OpenSSL/0.9.7l zlib/1.2.3
    "0F6163636570742D6C616E6775616765"    // accept-language
    "06656E2C206D69";                     // en, mi
constexpr absl::string_view kIndeterminateLengthEncodedRequestBodyChunks =
    "066368756E6B31"   // chunk1
    "066368756E6B32"   // chunk2
    "066368756E6B33";  // chunk3
constexpr absl::string_view kIndeterminateLengthEncodedRequestTrailers =
    "08747261696C657231"  // trailer1
    "0676616C756531"      // value1
    "08747261696C657232"  // trailer2
    "0676616C756532";     // value2
// Test examples from
// https://www.ietf.org/archive/id/draft-ietf-httpbis-binary-message-06.html

TEST(BinaryHttpRequest, EncodeGetNoBody) {
  /*
    GET /hello.txt HTTP/1.1
    User-Agent: curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3
    Host: www.example.com
    Accept-Language: en, mi
  */
  BinaryHttpRequest request({"GET", "https", "www.example.com", "/hello.txt"});
  request
      .AddHeaderField({"User-Agent",
                       "curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3"})
      ->AddHeaderField({"Host", "www.example.com"})
      ->AddHeaderField({"Accept-Language", "en, mi"});
  /*
      00000000: 00034745 54056874 74707300 0a2f6865  ..GET.https../he
      00000010: 6c6c6f2e 74787440 6c0a7573 65722d61  llo.txt@l.user-a
      00000020: 67656e74 34637572 6c2f372e 31362e33  gent4curl/7.16.3
      00000030: 206c6962 6375726c 2f372e31 362e3320   libcurl/7.16.3
      00000040: 4f70656e 53534c2f 302e392e 376c207a  OpenSSL/0.9.7l z
      00000050: 6c69622f 312e322e 3304686f 73740f77  lib/1.2.3.host.w
      00000060: 77772e65 78616d70 6c652e63 6f6d0f61  ww.example.com.a
      00000070: 63636570 742d6c61 6e677561 67650665  ccept-language.e
      00000080: 6e2c206d 6900                        n, mi..
  */
  const uint32_t expected_words[] = {
      0x00034745, 0x54056874, 0x74707300, 0x0a2f6865, 0x6c6c6f2e, 0x74787440,
      0x6c0a7573, 0x65722d61, 0x67656e74, 0x34637572, 0x6c2f372e, 0x31362e33,
      0x206c6962, 0x6375726c, 0x2f372e31, 0x362e3320, 0x4f70656e, 0x53534c2f,
      0x302e392e, 0x376c207a, 0x6c69622f, 0x312e322e, 0x3304686f, 0x73740f77,
      0x77772e65, 0x78616d70, 0x6c652e63, 0x6f6d0f61, 0x63636570, 0x742d6c61,
      0x6e677561, 0x67650665, 0x6e2c206d, 0x69000000};
  std::string expected;
  for (const auto& word : expected_words) {
    expected += WordToBytes(word);
  }
  // Remove padding.
  expected.resize(expected.size() - 2);

  const auto result = request.Serialize();
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(*result, expected);
  EXPECT_THAT(
      request.DebugString(),
      StrEq("BinaryHttpRequest{BinaryHttpMessage{Headers{Field{user-agent=curl/"
            "7.16.3 "
            "libcurl/7.16.3 OpenSSL/0.9.7l "
            "zlib/1.2.3};Field{host=www.example.com};Field{accept-language=en, "
            "mi}}Body{}}}"));
  QUICHE_EXPECT_OK(TestPrintTo(request));
}

TEST(BinaryHttpRequest, DecodeGetNoBody) {
  absl::string_view known_length_request =
      "80000000"  // 4-byte framing. The framing indicator is normally encoded
                  // using a single byte but we intentionally use a
                  // multiple-byte value here to test the decoding logic.
      "03474554"  // :method = GET
      "056874747073"            // :scheme = https
      "00"                      // :authority = ""
      "0A2F68656C6C6F2E747874"  // :path = /hello.txt
      "406C"                    // headers section length
      "0A757365722D6167656E74"  // user-agent
      "346375726C2F372E31362E33206C69626375726C2F372E31362E33204F70656E53534C2F"
      "302E392E376C207A6C69622F312E322E33"  // curl/7.16.3 libcurl/7.16.3
                                            // OpenSSL/0.9.7l zlib/1.2.3
      "04686F7374"                          // host
      "0F7777772E6578616D706C652E636F6D"    // www.example.com
      "0F6163636570742D6C616E6775616765"    // accept-language
      "06656E2C206D69"                      // en, mi
      "000000";                             // padding

  std::string data;
  ASSERT_TRUE(absl::HexStringToBytes(known_length_request, &data));
  const auto request_so = BinaryHttpRequest::Create(data);
  ASSERT_TRUE(request_so.ok());
  const BinaryHttpRequest request = *request_so;
  ASSERT_THAT(request.control_data(),
              FieldsAre("GET", "https", "", "/hello.txt"));
  std::vector<BinaryHttpMessage::Field> expected_fields = {
      {"user-agent", "curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3"},
      {"host", "www.example.com"},
      {"accept-language", "en, mi"}};
  for (const auto& field : expected_fields) {
    QUICHE_EXPECT_OK(TestPrintTo(field));
  }
  ASSERT_THAT(request.GetHeaderFields(), ContainerEq(expected_fields));
  ASSERT_EQ(request.body(), "");
  EXPECT_THAT(
      request.DebugString(),
      StrEq("BinaryHttpRequest{BinaryHttpMessage{Headers{Field{user-agent=curl/"
            "7.16.3 "
            "libcurl/7.16.3 OpenSSL/0.9.7l "
            "zlib/1.2.3};Field{host=www.example.com};Field{accept-language=en, "
            "mi}}Body{}}}"));
  QUICHE_EXPECT_OK(TestPrintTo(request));
}

TEST(BinaryHttpRequest, EncodeGetNoBodyOrHeaders) {
  /*
    (HTTP/2)
    :method GET
    :authority example.com
    :path /
    :scheme https
  */
  BinaryHttpRequest request({"GET", "https", "example.com", "/"});

  /*
      00000000: 00034745 54056874 7470730b 6578616d  ..GET.https.exam
      00000010: 706c652e 636f6d01 2f000              ple.com./..
  */
  const uint32_t expected_words[] = {0x00034745, 0x54056874, 0x7470730b,
                                     0x6578616d, 0x706c652e, 0x636f6d01,
                                     0x2f000000};
  std::string expected;
  for (const auto& word : expected_words) {
    expected += WordToBytes(word);
  }
  // Remove padding.
  expected.resize(expected.size() - 1);

  const auto result = request.Serialize();
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(*result, expected);
  EXPECT_THAT(request.DebugString(),
              StrEq("BinaryHttpRequest{BinaryHttpMessage{Headers{}Body{}}}"));
  QUICHE_EXPECT_OK(TestPrintTo(request));
}

TEST(BinaryHttpRequest, DecodeGetNoBodyOrHeaders) {
  const uint32_t words[] = {0x00034745, 0x54056874, 0x7470730b, 0x6578616d,
                            0x706c652e, 0x636f6d01, 0x2f000000};
  std::string data;
  for (const auto& word : words) {
    data += WordToBytes(word);
  }

  for (int i = 0; i < 3; ++i) {
    // In the first pass, we remove a byte of padding, or alternatively an empty
    // set of trailers. In the second pass, we further omit the empty body. In
    // the third pass, we further omit the empty headers.
    data.resize(data.size() - 1);
    const auto request_so = BinaryHttpRequest::Create(data);
    ASSERT_TRUE(request_so.ok());
    const BinaryHttpRequest request = *request_so;
    ASSERT_THAT(request.control_data(),
                FieldsAre("GET", "https", "example.com", "/"));
    std::vector<BinaryHttpMessage::Field> expected_fields = {};
    for (const auto& field : expected_fields) {
      QUICHE_EXPECT_OK(TestPrintTo(field));
    }
    ASSERT_THAT(request.GetHeaderFields(), ContainerEq(expected_fields));
    ASSERT_EQ(request.body(), "");
    EXPECT_THAT(request.DebugString(),
                StrEq("BinaryHttpRequest{BinaryHttpMessage{Headers{}Body{}}}"));
    QUICHE_EXPECT_OK(TestPrintTo(request));
  }
}

TEST(BinaryHttpRequest, EncodeGetWithAuthority) {
  /*
    GET https://www.example.com/hello.txt HTTP/1.1
    User-Agent: curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3
    Accept-Language: en, mi
  */
  BinaryHttpRequest request({"GET", "https", "www.example.com", "/hello.txt"});
  request
      .AddHeaderField({"User-Agent",
                       "curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3"})
      ->AddHeaderField({"Accept-Language", "en, mi"});
  /*
    00000000: 00034745 54056874 7470730f 7777772e  ..GET.https.www.
    00000010: 6578616d 706c652e 636f6d0a 2f68656c  example.com./hel
    00000020: 6c6f2e74 78744057 0a757365 722d6167  lo.txt@W.user-ag
    00000030: 656e7434 6375726c 2f372e31 362e3320  ent4curl/7.16.3
    00000040: 6c696263 75726c2f 372e3136 2e33204f  libcurl/7.16.3 O
    00000050: 70656e53 534c2f30 2e392e37 6c207a6c  penSSL/0.9.7l zl
    00000060: 69622f31 2e322e33 0f616363 6570742d  ib/1.2.3.accept-
    00000070: 6c616e67 75616765 06656e2c 206d6900  language.en, mi.
  */

  const uint32_t expected_words[] = {
      0x00034745, 0x54056874, 0x7470730f, 0x7777772e, 0x6578616d, 0x706c652e,
      0x636f6d0a, 0x2f68656c, 0x6c6f2e74, 0x78744057, 0x0a757365, 0x722d6167,
      0x656e7434, 0x6375726c, 0x2f372e31, 0x362e3320, 0x6c696263, 0x75726c2f,
      0x372e3136, 0x2e33204f, 0x70656e53, 0x534c2f30, 0x2e392e37, 0x6c207a6c,
      0x69622f31, 0x2e322e33, 0x0f616363, 0x6570742d, 0x6c616e67, 0x75616765,
      0x06656e2c, 0x206d6900};
  std::string expected;
  for (const auto& word : expected_words) {
    expected += WordToBytes(word);
  }
  const auto result = request.Serialize();
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(*result, expected);
  EXPECT_THAT(
      request.DebugString(),
      StrEq("BinaryHttpRequest{BinaryHttpMessage{Headers{Field{user-agent=curl/"
            "7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l "
            "zlib/1.2.3};Field{accept-language=en, mi}}Body{}}}"));
}

TEST(BinaryHttpRequest, DecodeGetWithAuthority) {
  const uint32_t words[] = {
      0x00034745, 0x54056874, 0x7470730f, 0x7777772e, 0x6578616d, 0x706c652e,
      0x636f6d0a, 0x2f68656c, 0x6c6f2e74, 0x78744057, 0x0a757365, 0x722d6167,
      0x656e7434, 0x6375726c, 0x2f372e31, 0x362e3320, 0x6c696263, 0x75726c2f,
      0x372e3136, 0x2e33204f, 0x70656e53, 0x534c2f30, 0x2e392e37, 0x6c207a6c,
      0x69622f31, 0x2e322e33, 0x0f616363, 0x6570742d, 0x6c616e67, 0x75616765,
      0x06656e2c, 0x206d6900, 0x00};
  std::string data;
  for (const auto& word : words) {
    data += WordToBytes(word);
  }
  const auto request_so = BinaryHttpRequest::Create(data);
  ASSERT_TRUE(request_so.ok());
  const BinaryHttpRequest request = *request_so;
  ASSERT_THAT(request.control_data(),
              FieldsAre("GET", "https", "www.example.com", "/hello.txt"));
  std::vector<BinaryHttpMessage::Field> expected_fields = {
      {"user-agent", "curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3"},
      {"accept-language", "en, mi"}};
  ASSERT_THAT(request.GetHeaderFields(), ContainerEq(expected_fields));
  ASSERT_EQ(request.body(), "");
  EXPECT_THAT(
      request.DebugString(),
      StrEq("BinaryHttpRequest{BinaryHttpMessage{Headers{Field{user-agent=curl/"
            "7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l "
            "zlib/1.2.3};Field{accept-language=en, mi}}Body{}}}"));
}

TEST(BinaryHttpRequest, EncodePostBody) {
  /*
  POST /hello.txt HTTP/1.1
  User-Agent: not/telling
  Host: www.example.com
  Accept-Language: en

  Some body that I used to post.
  */
  BinaryHttpRequest request({"POST", "https", "www.example.com", "/hello.txt"});
  request.AddHeaderField({"User-Agent", "not/telling"})
      ->AddHeaderField({"Host", "www.example.com"})
      ->AddHeaderField({"Accept-Language", "en"})
      ->set_body({"Some body that I used to post.\r\n"});
  /*
    00000000: 0004504f 53540568 74747073 000a2f68  ..POST.https../h
    00000010: 656c6c6f 2e747874 3f0a7573 65722d61  ello.txt?.user-a
    00000020: 67656e74 0b6e6f74 2f74656c 6c696e67  gent.not/telling
    00000030: 04686f73 740f7777 772e6578 616d706c  .host.www.exampl
    00000040: 652e636f 6d0f6163 63657074 2d6c616e  e.com.accept-lan
    00000050: 67756167 6502656e 20536f6d 6520626f  guage.en Some bo
    00000060: 64792074 68617420 49207573 65642074  dy that I used t
    00000070: 6f20706f 73742e0d 0a                 o post....
  */
  const uint32_t expected_words[] = {
      0x0004504f, 0x53540568, 0x74747073, 0x000a2f68, 0x656c6c6f, 0x2e747874,
      0x3f0a7573, 0x65722d61, 0x67656e74, 0x0b6e6f74, 0x2f74656c, 0x6c696e67,
      0x04686f73, 0x740f7777, 0x772e6578, 0x616d706c, 0x652e636f, 0x6d0f6163,
      0x63657074, 0x2d6c616e, 0x67756167, 0x6502656e, 0x20536f6d, 0x6520626f,
      0x64792074, 0x68617420, 0x49207573, 0x65642074, 0x6f20706f, 0x73742e0d,
      0x0a000000};
  std::string expected;
  for (const auto& word : expected_words) {
    expected += WordToBytes(word);
  }
  // Remove padding.
  expected.resize(expected.size() - 3);
  const auto result = request.Serialize();
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(*result, expected);
  EXPECT_THAT(
      request.DebugString(),
      StrEq("BinaryHttpRequest{BinaryHttpMessage{Headers{Field{user-agent=not/"
            "telling};Field{host=www.example.com};Field{accept-language=en}}"
            "Body{Some "
            "body that I used to post.\r\n}}}"));
}

TEST(BinaryHttpRequest, DecodePostBody) {
  const uint32_t words[] = {
      0x0004504f, 0x53540568, 0x74747073, 0x000a2f68, 0x656c6c6f, 0x2e747874,
      0x3f0a7573, 0x65722d61, 0x67656e74, 0x0b6e6f74, 0x2f74656c, 0x6c696e67,
      0x04686f73, 0x740f7777, 0x772e6578, 0x616d706c, 0x652e636f, 0x6d0f6163,
      0x63657074, 0x2d6c616e, 0x67756167, 0x6502656e, 0x20536f6d, 0x6520626f,
      0x64792074, 0x68617420, 0x49207573, 0x65642074, 0x6f20706f, 0x73742e0d,
      0x0a000000};
  std::string data;
  for (const auto& word : words) {
    data += WordToBytes(word);
  }
  const auto request_so = BinaryHttpRequest::Create(data);
  ASSERT_TRUE(request_so.ok());
  BinaryHttpRequest request = *request_so;
  ASSERT_THAT(request.control_data(),
              FieldsAre("POST", "https", "", "/hello.txt"));
  std::vector<BinaryHttpMessage::Field> expected_fields = {
      {"user-agent", "not/telling"},
      {"host", "www.example.com"},
      {"accept-language", "en"}};
  ASSERT_THAT(request.GetHeaderFields(), ContainerEq(expected_fields));
  ASSERT_EQ(request.body(), "Some body that I used to post.\r\n");
  EXPECT_THAT(
      request.DebugString(),
      StrEq("BinaryHttpRequest{BinaryHttpMessage{Headers{Field{user-agent=not/"
            "telling};Field{host=www.example.com};Field{accept-language=en}}"
            "Body{Some "
            "body that I used to post.\r\n}}}"));
}

TEST(BinaryHttpRequest, Equality) {
  BinaryHttpRequest request({"POST", "https", "www.example.com", "/hello.txt"});
  request.AddHeaderField({"User-Agent", "not/telling"})
      ->set_body({"hello, world!\r\n"});

  BinaryHttpRequest same({"POST", "https", "www.example.com", "/hello.txt"});
  same.AddHeaderField({"User-Agent", "not/telling"})
      ->set_body({"hello, world!\r\n"});
  EXPECT_EQ(request, same);
}

TEST(BinaryHttpRequest, Inequality) {
  BinaryHttpRequest request({"POST", "https", "www.example.com", "/hello.txt"});
  request.AddHeaderField({"User-Agent", "not/telling"})
      ->set_body({"hello, world!\r\n"});

  BinaryHttpRequest different_control(
      {"PUT", "https", "www.example.com", "/hello.txt"});
  different_control.AddHeaderField({"User-Agent", "not/telling"})
      ->set_body({"hello, world!\r\n"});
  EXPECT_NE(request, different_control);

  BinaryHttpRequest different_header(
      {"PUT", "https", "www.example.com", "/hello.txt"});
  different_header.AddHeaderField({"User-Agent", "told/you"})
      ->set_body({"hello, world!\r\n"});
  EXPECT_NE(request, different_header);

  BinaryHttpRequest no_header(
      {"PUT", "https", "www.example.com", "/hello.txt"});
  no_header.set_body({"hello, world!\r\n"});
  EXPECT_NE(request, no_header);

  BinaryHttpRequest different_body(
      {"POST", "https", "www.example.com", "/hello.txt"});
  different_body.AddHeaderField({"User-Agent", "not/telling"})
      ->set_body({"goodbye, world!\r\n"});
  EXPECT_NE(request, different_body);

  BinaryHttpRequest no_body({"POST", "https", "www.example.com", "/hello.txt"});
  no_body.AddHeaderField({"User-Agent", "not/telling"});
  EXPECT_NE(request, no_body);
}

absl::Status ExpectRequestMessageSectionHandler(
    const RequestMessageSectionTestHandler::MessageData& message_data) {
  if (!message_data.control_data_.has_value()) {
    return absl::FailedPreconditionError("control_data missing");
  }
  if (message_data.control_data_->method != "POST" ||
      message_data.control_data_->scheme != "https" ||
      message_data.control_data_->authority != "google.com" ||
      message_data.control_data_->path != "/hello") {
    return absl::FailedPreconditionError("control_data mismatch");
  }
  std::vector<std::pair<std::string, std::string>> expected_headers = {
      {"user-agent", "curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3"},
      {"accept-language", "en, mi"}};
  if (!message_data.headers_done_) {
    return absl::FailedPreconditionError("headers not done");
  }
  if (message_data.headers_ != expected_headers) {
    return absl::FailedPreconditionError("headers mismatch");
  }
  std::vector<std::string> expected_body_chunks = {"chunk1", "chunk2",
                                                   "chunk3"};
  if (!message_data.body_chunks_done_) {
    return absl::FailedPreconditionError("body chunks not done");
  }
  if (message_data.body_chunks_ != expected_body_chunks) {
    return absl::FailedPreconditionError("body chunks mismatch");
  }
  std::vector<std::pair<std::string, std::string>> expected_trailers = {
      {"trailer1", "value1"}, {"trailer2", "value2"}};
  if (!message_data.trailers_done_) {
    return absl::FailedPreconditionError("trailers not done");
  }
  if (message_data.trailers_ != expected_trailers) {
    return absl::FailedPreconditionError("trailers mismatch");
  }
  return absl::OkStatus();
}

TEST(IndeterminateLengthDecoder, FullRequestDecodingSuccess) {
  std::string request_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(
          k2ByteFramingIndicator, kIndeterminateLengthEncodedRequestControlData,
          kIndeterminateLengthEncodedRequestHeaders, k8ByteContentTerminator,
          kIndeterminateLengthEncodedRequestBodyChunks, k4ByteContentTerminator,
          kIndeterminateLengthEncodedRequestTrailers, kContentTerminator,
          kPadding),
      &request_bytes));
  RequestMessageSectionTestHandler handler;
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(&handler);
  QUICHE_EXPECT_OK(decoder.Decode(request_bytes, /*end_stream=*/true));
  QUICHE_EXPECT_OK(
      ExpectRequestMessageSectionHandler(handler.GetMessageData()));
}

class MockFailingMessageSectionHandler
    : public BinaryHttpRequest::IndeterminateLengthDecoder::
          MessageSectionHandler {
 public:
  MOCK_METHOD(absl::Status, OnControlData,
              (const BinaryHttpRequest::ControlData& control_data), (override));

  MOCK_METHOD(absl::Status, OnHeader,
              (absl::string_view name, absl::string_view value), (override));
  MOCK_METHOD(absl::Status, OnHeadersDone, (), (override));
  MOCK_METHOD(absl::Status, OnBodyChunk, (absl::string_view body_chunk),
              (override));
  MOCK_METHOD(absl::Status, OnBodyChunksDone, (), (override));
  MOCK_METHOD(absl::Status, OnTrailer,
              (absl::string_view name, absl::string_view value), (override));
  MOCK_METHOD(absl::Status, OnTrailersDone, (), (override));
};

std::unique_ptr<MockFailingMessageSectionHandler>
GetMockMessageSectionHandler() {
  auto handler = std::make_unique<MockFailingMessageSectionHandler>();
  ON_CALL(*handler, OnControlData(_)).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnHeader(_, _)).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnHeadersDone()).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnBodyChunk(_)).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnBodyChunksDone()).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnTrailer(_, _)).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnTrailersDone()).WillByDefault(Return(absl::OkStatus()));
  return handler;
}

TEST(IndeterminateLengthDecoder, FailedMessageSectionHandler) {
  std::string request_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(
          k2ByteFramingIndicator, kIndeterminateLengthEncodedRequestControlData,
          kIndeterminateLengthEncodedRequestHeaders, k8ByteContentTerminator,
          kIndeterminateLengthEncodedRequestBodyChunks, k4ByteContentTerminator,
          kIndeterminateLengthEncodedRequestTrailers, kContentTerminator,
          kPadding),
      &request_bytes));

  std::unique_ptr<MockFailingMessageSectionHandler> handler =
      GetMockMessageSectionHandler();
  std::string error_message = "Failed to handle control data";
  EXPECT_CALL(*handler, OnControlData(_))
      .WillOnce(Return(absl::InternalError(error_message)));
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(handler.get());
  EXPECT_THAT(decoder.Decode(request_bytes, true),
              test::StatusIs(kInternal, HasSubstr(error_message)));

  handler = GetMockMessageSectionHandler();
  error_message = "Failed to handle header";
  EXPECT_CALL(*handler, OnHeader(_, _))
      .WillOnce(Return(absl::InternalError(error_message)));
  BinaryHttpRequest::IndeterminateLengthDecoder decoder2(handler.get());
  EXPECT_THAT(decoder2.Decode(request_bytes, true),
              test::StatusIs(kInternal, HasSubstr(error_message)));

  handler = GetMockMessageSectionHandler();
  error_message = "Failed to handle headers done";
  EXPECT_CALL(*handler, OnHeadersDone())
      .WillOnce(Return(absl::InternalError(error_message)));
  BinaryHttpRequest::IndeterminateLengthDecoder decoder3(handler.get());
  EXPECT_THAT(decoder3.Decode(request_bytes, true),
              test::StatusIs(kInternal, HasSubstr(error_message)));

  handler = GetMockMessageSectionHandler();
  error_message = "Failed to handle body chunk";
  EXPECT_CALL(*handler, OnBodyChunk(_))
      .WillOnce(Return(absl::InternalError(error_message)));
  BinaryHttpRequest::IndeterminateLengthDecoder decoder4(handler.get());
  EXPECT_THAT(decoder4.Decode(request_bytes, true),
              test::StatusIs(kInternal, HasSubstr(error_message)));

  handler = GetMockMessageSectionHandler();
  error_message = "Failed to handle body chunks done";
  EXPECT_CALL(*handler, OnBodyChunksDone())
      .WillOnce(Return(absl::InternalError(error_message)));
  BinaryHttpRequest::IndeterminateLengthDecoder decoder5(handler.get());
  EXPECT_THAT(decoder5.Decode(request_bytes, true),
              test::StatusIs(kInternal, HasSubstr(error_message)));

  handler = GetMockMessageSectionHandler();
  error_message = "Failed to handle trailer";
  EXPECT_CALL(*handler, OnTrailer(_, _))
      .WillOnce(Return(absl::InternalError(error_message)));
  BinaryHttpRequest::IndeterminateLengthDecoder decoder6(handler.get());
  EXPECT_THAT(decoder6.Decode(request_bytes, true),
              test::StatusIs(kInternal, HasSubstr(error_message)));

  handler = GetMockMessageSectionHandler();
  error_message = "Failed to handle trailers done";
  EXPECT_CALL(*handler, OnTrailersDone())
      .WillOnce(Return(absl::InternalError(error_message)));
  BinaryHttpRequest::IndeterminateLengthDecoder decoder7(handler.get());
  EXPECT_THAT(decoder7.Decode(request_bytes, true),
              test::StatusIs(kInternal, HasSubstr(error_message)));
}

TEST(IndeterminateLengthDecoder, BufferedRequestDecodingSuccess) {
  std::string request_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(
          k2ByteFramingIndicator, kIndeterminateLengthEncodedRequestControlData,
          kIndeterminateLengthEncodedRequestHeaders, k8ByteContentTerminator,
          kIndeterminateLengthEncodedRequestBodyChunks, k4ByteContentTerminator,
          kIndeterminateLengthEncodedRequestTrailers, kContentTerminator,
          kPadding),
      &request_bytes));
  RequestMessageSectionTestHandler handler;
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(&handler);
  for (uint64_t i = 0; i < request_bytes.size() - 1; i++) {
    QUICHE_EXPECT_OK(
        decoder.Decode(absl::string_view(&request_bytes[i], 1), false));
  }
  // Decode the last byte, send end_stream.
  QUICHE_EXPECT_OK(decoder.Decode(
      absl::string_view(&request_bytes[request_bytes.size() - 1], 1), true));
  QUICHE_EXPECT_OK(
      ExpectRequestMessageSectionHandler(handler.GetMessageData()));
}

TEST(IndeterminateLengthDecoder,
     OutOfRangeTreatedAsInvalidArgumentWhenEndStream) {
  std::string incomplete_request_bytes =
      "4002"         // 2-byte framing indicator
      "04504F5354";  // :method = POST
  std::string request_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(incomplete_request_bytes, &request_bytes));
  RequestMessageSectionTestHandler handler;
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(&handler);
  EXPECT_THAT(decoder.Decode(request_bytes, /*end_stream=*/true),
              test::StatusIs(kInvalidArgument));
}

TEST(IndeterminateLengthDecoder, InvalidFramingError) {
  RequestMessageSectionTestHandler handler;
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(&handler);
  std::string request_bytes;
  EXPECT_TRUE(absl::HexStringToBytes("00", &request_bytes));
  absl::Status status = decoder.Decode(request_bytes, /*end_stream=*/false);
  EXPECT_THAT(status, test::StatusIs(kInvalidArgument));
}

TEST(IndeterminateLengthDecoder, InvalidPaddingError) {
  RequestMessageSectionTestHandler handler;
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(&handler);
  std::string request_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(
          k2ByteFramingIndicator, kIndeterminateLengthEncodedRequestControlData,
          kIndeterminateLengthEncodedRequestHeaders, k8ByteContentTerminator,
          kIndeterminateLengthEncodedRequestBodyChunks, k4ByteContentTerminator,
          kIndeterminateLengthEncodedRequestTrailers, kContentTerminator,
          kPadding),
      &request_bytes));
  QUICHE_EXPECT_OK(decoder.Decode(request_bytes, /*end_stream=*/false));
  absl::Status status = decoder.Decode("\x01", /*end_stream=*/false);
  EXPECT_THAT(status, test::StatusIs(kInvalidArgument));
}

absl::Status ExpectTruncatedTrailerSection(
    const RequestMessageSectionTestHandler::MessageData& message_data) {
  if (!message_data.headers_done_) {
    return absl::FailedPreconditionError("headers not done");
  }
  if (!message_data.trailers_done_) {
    return absl::FailedPreconditionError("trailers not done");
  }
  std::vector<std::pair<std::string, std::string>> expected_headers = {
      {"user-agent", "curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3"},
      {"accept-language", "en, mi"}};
  if (message_data.headers_ != expected_headers) {
    return absl::FailedPreconditionError("headers mismatch");
  }
  if (!message_data.trailers_.empty()) {
    return absl::FailedPreconditionError("trailers not empty");
  }
  return absl::OkStatus();
}

TEST(IndeterminateLengthDecoder, TruncatedBodyAndTrailers) {
  RequestMessageSectionTestHandler handler;
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(&handler);
  std::string request_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(
          k2ByteFramingIndicator, kIndeterminateLengthEncodedRequestControlData,
          kIndeterminateLengthEncodedRequestHeaders, k8ByteContentTerminator),
      &request_bytes));

  QUICHE_EXPECT_OK(decoder.Decode(request_bytes, /*end_stream=*/true));
  auto message_data = handler.GetMessageData();
  EXPECT_TRUE(message_data.body_chunks_done_);
  EXPECT_THAT(message_data.body_chunks_, IsEmpty());
  QUICHE_EXPECT_OK(ExpectTruncatedTrailerSection(message_data));
}

TEST(IndeterminateLengthDecoder, TruncatedBodyAndTrailersSplitEndStream) {
  RequestMessageSectionTestHandler handler;
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(&handler);
  std::string request_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(
          k2ByteFramingIndicator, kIndeterminateLengthEncodedRequestControlData,
          kIndeterminateLengthEncodedRequestHeaders, k8ByteContentTerminator),
      &request_bytes));

  QUICHE_EXPECT_OK(decoder.Decode(request_bytes, /*end_stream=*/false));
  // Send `end_stream` with no data.
  QUICHE_EXPECT_OK(decoder.Decode("", /*end_stream=*/true));
  auto message_data = handler.GetMessageData();
  EXPECT_TRUE(message_data.body_chunks_done_);
  EXPECT_THAT(message_data.body_chunks_, IsEmpty());
  QUICHE_EXPECT_OK(ExpectTruncatedTrailerSection(message_data));
}

struct RequestIndeterminateLengthEncoderTestData {
  BinaryHttpRequest::ControlData control_data{"POST", "https", "google.com",
                                              "/hello"};
  std::vector<BinaryHttpMessage::FieldView> headers{
      {"User-Agent", "curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3"},
      {"accept-language", "en, mi"}};
  std::vector<absl::string_view> body_chunks = {"chunk1", "chunk2", "chunk3"};
  std::vector<BinaryHttpMessage::FieldView> trailers{{"trailer1", "value1"},
                                                     {"trailer2", "value2"}};
};

TEST(RequestIndeterminateLengthEncoder, FullRequest) {
  std::string expected;
  ASSERT_TRUE(absl::HexStringToBytes(
      absl::StrCat(
          kFramingIndicator, kIndeterminateLengthEncodedRequestControlData,
          kIndeterminateLengthEncodedRequestHeaders, kContentTerminator,
          kIndeterminateLengthEncodedRequestBodyChunks, kContentTerminator,
          kIndeterminateLengthEncodedRequestTrailers, kContentTerminator),
      &expected));

  BinaryHttpRequest::IndeterminateLengthEncoder encoder;
  RequestIndeterminateLengthEncoderTestData test_data;
  std::string encoded_data;

  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeControlData(test_data.control_data);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }

  status_or_encoded_data = encoder.EncodeHeaders(test_data.headers);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }

  status_or_encoded_data =
      encoder.EncodeBodyChunks(test_data.body_chunks, true);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }

  status_or_encoded_data = encoder.EncodeTrailers(test_data.trailers);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }

  EXPECT_EQ(encoded_data, expected);
}

TEST(RequestIndeterminateLengthEncoder, RequestNoBody) {
  std::string expected;
  ASSERT_TRUE(absl::HexStringToBytes(
      absl::StrCat(
          kFramingIndicator, kIndeterminateLengthEncodedRequestControlData,
          kIndeterminateLengthEncodedRequestHeaders, kContentTerminator,
          kContentTerminator,   // Empty body chunks
          kContentTerminator),  // Empty trailers
      &expected));

  BinaryHttpRequest::IndeterminateLengthEncoder encoder;
  RequestIndeterminateLengthEncoderTestData test_data;
  std::string encoded_data;

  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeControlData(test_data.control_data);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }

  status_or_encoded_data = encoder.EncodeHeaders(test_data.headers);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }

  status_or_encoded_data = encoder.EncodeBodyChunks({}, true);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }

  status_or_encoded_data = encoder.EncodeTrailers({});
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }

  EXPECT_EQ(encoded_data, expected);
}

TEST(RequestIndeterminateLengthEncoder, EncodingChunksMultipleTimes) {
  std::string expected;
  ASSERT_TRUE(absl::HexStringToBytes(
      absl::StrCat(
          kFramingIndicator, kIndeterminateLengthEncodedRequestControlData,
          kIndeterminateLengthEncodedRequestHeaders, kContentTerminator,
          kIndeterminateLengthEncodedRequestBodyChunks, kContentTerminator,
          kIndeterminateLengthEncodedRequestTrailers, kContentTerminator),
      &expected));

  BinaryHttpRequest::IndeterminateLengthEncoder encoder;
  RequestIndeterminateLengthEncoderTestData test_data;
  std::string encoded_data;

  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeControlData(test_data.control_data);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }

  status_or_encoded_data = encoder.EncodeHeaders(test_data.headers);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }

  status_or_encoded_data =
      encoder.EncodeBodyChunks({test_data.body_chunks[0]}, false);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }
  status_or_encoded_data =
      encoder.EncodeBodyChunks({test_data.body_chunks[1]}, false);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }
  status_or_encoded_data =
      encoder.EncodeBodyChunks({test_data.body_chunks[2]}, false);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }
  status_or_encoded_data = encoder.EncodeBodyChunks({}, true);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }

  status_or_encoded_data = encoder.EncodeTrailers(test_data.trailers);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  if (status_or_encoded_data.ok()) {
    encoded_data += *status_or_encoded_data;
  }
  EXPECT_EQ(encoded_data, expected);

  RequestMessageSectionTestHandler handler;
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(&handler);
  QUICHE_EXPECT_OK(decoder.Decode(encoded_data, /*end_stream=*/true));
  QUICHE_EXPECT_OK(
      ExpectRequestMessageSectionHandler(handler.GetMessageData()));
}

TEST(RequestIndeterminateLengthEncoder, OutOfOrderHeaders) {
  BinaryHttpRequest::IndeterminateLengthEncoder encoder;
  EXPECT_THAT(encoder.EncodeHeaders({}), test::StatusIs(kInvalidArgument));
}

TEST(RequestIndeterminateLengthEncoder, OutOfOrderBodyChunks) {
  BinaryHttpRequest::IndeterminateLengthEncoder encoder;
  EXPECT_THAT(encoder.EncodeBodyChunks({}, true),
              test::StatusIs(kInvalidArgument));
}

TEST(RequestIndeterminateLengthEncoder, OutOfOrderTrailers) {
  BinaryHttpRequest::IndeterminateLengthEncoder encoder;
  EXPECT_THAT(encoder.EncodeTrailers({}), test::StatusIs(kInvalidArgument));
}

TEST(RequestIndeterminateLengthEncoder, MustNotEncodeControlDataTwice) {
  BinaryHttpRequest::IndeterminateLengthEncoder encoder;
  RequestIndeterminateLengthEncoderTestData test_data;
  QUICHE_EXPECT_OK(encoder.EncodeControlData(test_data.control_data));
  EXPECT_THAT(encoder.EncodeControlData(test_data.control_data),
              test::StatusIs(kInvalidArgument));
}

TEST(RequestIndeterminateLengthEncoder, MustNotEncodeHeadersTwice) {
  BinaryHttpRequest::IndeterminateLengthEncoder encoder;
  RequestIndeterminateLengthEncoderTestData test_data;
  QUICHE_EXPECT_OK(encoder.EncodeControlData(test_data.control_data));
  QUICHE_EXPECT_OK(encoder.EncodeHeaders({}));
  EXPECT_THAT(encoder.EncodeHeaders({}), test::StatusIs(kInvalidArgument));
}

TEST(RequestIndeterminateLengthEncoder, MustNotEncodeChunksAfterChunksDone) {
  BinaryHttpRequest::IndeterminateLengthEncoder encoder;
  RequestIndeterminateLengthEncoderTestData test_data;
  QUICHE_EXPECT_OK(encoder.EncodeControlData(test_data.control_data));
  QUICHE_EXPECT_OK(encoder.EncodeHeaders({}));
  QUICHE_EXPECT_OK(encoder.EncodeBodyChunks({}, true));
  EXPECT_THAT(encoder.EncodeBodyChunks({}, true),
              test::StatusIs(kInvalidArgument));
}

TEST(RequestIndeterminateLengthEncoder, MustNotEncodeTrailersTwice) {
  BinaryHttpRequest::IndeterminateLengthEncoder encoder;
  RequestIndeterminateLengthEncoderTestData test_data;
  QUICHE_EXPECT_OK(encoder.EncodeControlData(test_data.control_data));
  QUICHE_EXPECT_OK(encoder.EncodeHeaders({}));
  QUICHE_EXPECT_OK(encoder.EncodeBodyChunks({}, true));
  QUICHE_EXPECT_OK(encoder.EncodeTrailers({}));
  EXPECT_THAT(encoder.EncodeTrailers({}), test::StatusIs(kInvalidArgument));
}

struct ResponseIndeterminateLengthEncoderTestData {
  std::vector<quiche::BinaryHttpMessage::FieldView> informationalResponse1{
      {"running", "\"sleep 15\""}};
  std::vector<BinaryHttpMessage::FieldView> informationalResponse2{
      {/*uppercase*/ "LINK", "</style.css>; rel=preload; as=style"},
      {"link", "</script.js>; rel=preload; as=script"},
      {"longer_header_value",
       "1111111111111111111111111111111111111111111111111111111111111111"}};
  std::vector<BinaryHttpMessage::FieldView> headers{
      {"Date", "Mon, 27 Jul 2009 12:28:53 GMT"},
      {"Server", "Apache"},
      {"longer_header_value",
       "1111111111111111111111111111111111111111111111111111111111111111"}};
  std::vector<absl::string_view> body_chunks = {
      "chunk1", "chunk2", "chunk3",
      "1111111111111111111111111111111111111111111111111111111111111111"};
  std::vector<BinaryHttpMessage::FieldView> trailers{
      {"trailer1", "value1"},
      {"trailer2", "value2"},
      {"longer_trailer_value",
       "1111111111111111111111111111111111111111111111111111111111111111"}};
};

constexpr absl::string_view kIndeterminateLengthResponseFramingIndicator = "03";
constexpr absl::string_view kInfoResp1StatusCode = "4066";  // status code: 102
constexpr absl::string_view kInfoResp1Headers =
    "0772756e6e696e67"                                      // running
    "0a22736C65657020313522";                               // "sleep 15"
constexpr absl::string_view kInfoResp2StatusCode = "4067";  // status code: 103
constexpr absl::string_view kInfoResp2Headers =
    "046C696E6B"  // link
    "233C2F7374796C652E6373733E3B2072656C3D7072656C6F6"
    "1643B2061733D737479"
    "6C65"        // </style.css>; rel=preload; as=style
    "046C696E6B"  // link
    "243C2F7363726970742E6A733E3B2072656C3D7072656C6F6"
    "1643B2061733D736372"
    "697074"  // </script.js>; rel=preload; as=script
    "136C6F6E6765725F6865616465725F76616C7565"  // longer_header_value
    "40403131313131313131313131313131313131313131313131313131313131313131313131"
    "3131313131313131313131313131313131313131313131313131313131";  // 64 1s

constexpr absl::string_view kFinalResponseStatusCode = "40C8";  // 200
constexpr absl::string_view kFinalResponseHeaders =
    "0464617465"  // date
    "1D4D6F6E2C203237204A756C20323030392031323A32383A3"
    "53320474D54"                               // Mon, 27
                                                // Jul
                                                // 2009
                                                // 12:28:53
                                                // GMT
    "06736572766572"                            // server
    "06417061636865"                            // Apache
    "136C6F6E6765725F6865616465725F76616C7565"  // longer_header_value
    "40403131313131313131313131313131313131313131313131313131313131313131313131"
    "3131313131313131313131313131313131313131313131313131313131";  // 64 1s
constexpr absl::string_view kFinalResponseBody =
    "066368756E6B31"  // chunk1
    "066368756E6B32"  // chunk2
    "066368756E6B33"  // chunk3
    "40403131313131313131313131313131313131313131313131313131313131313131313131"
    "3131313131313131313131313131313131313131313131313131313131";  // 64 1s
constexpr absl::string_view kFinalResponseTrailers =
    "08747261696C657231"                          // trailer1
    "0676616C756531"                              // value1
    "08747261696C657232"                          // trailer2
    "0676616C756532"                              // value2
    "146C6F6E6765725F747261696C65725F76616C7565"  // longer_trailer_value
    "40403131313131313131313131313131313131313131313131313131313131313131313131"
    "3131313131313131313131313131313131313131313131313131313131";  // 64 1s

class ResponseMessageSectionTestHandler
    : public BinaryHttpResponse::IndeterminateLengthDecoder::
          MessageSectionHandler {
 public:
  struct InformationalResponse {
    uint16_t status_code = 0;
    std::vector<std::pair<std::string /*name*/, std::string /*value*/>> headers;
  };
  struct MessageData {
    std::vector<InformationalResponse> informational_responses;
    bool informational_responses_section_done = false;
    uint16_t final_status_code = 0;
    std::vector<std::pair<std::string /*name*/, std::string /*value*/>> headers;
    bool headers_done = false;
    std::vector<std::string> body_chunks;
    bool body_chunks_done = false;
    std::vector<std::pair<std::string /*name*/, std::string /*value*/>>
        trailers;
    bool trailers_done = false;
  };

  absl::Status OnInformationalResponseStatusCode(
      uint16_t status_code) override {
    if (message_data_.informational_responses_section_done) {
      return absl::FailedPreconditionError(
          "OnInformationalResponseStatusCode after section done");
    }
    message_data_.informational_responses.emplace_back(
        status_code,
        /*headers=*/std::vector<
            std::pair<std::string /*name*/, std::string /*value*/>>());
    return absl::OkStatus();
  }

  absl::Status OnInformationalResponseHeader(absl::string_view name,
                                             absl::string_view value) override {
    if (message_data_.informational_responses.empty()) {
      return absl::FailedPreconditionError(
          "OnInformationalResponseHeader called with no informational "
          "response");
    }
    message_data_.informational_responses.back().headers.emplace_back(
        std::string(name), std::string(value));
    return absl::OkStatus();
  }

  absl::Status OnInformationalResponseDone() override {
    return absl::OkStatus();
  }

  absl::Status OnInformationalResponsesSectionDone() override {
    message_data_.informational_responses_section_done = true;
    return absl::OkStatus();
  }

  absl::Status OnFinalResponseStatusCode(uint16_t status_code) override {
    message_data_.informational_responses_section_done = true;
    message_data_.final_status_code = status_code;
    return absl::OkStatus();
  }

  absl::Status OnFinalResponseHeader(absl::string_view name,
                                     absl::string_view value) override {
    if (message_data_.headers_done) {
      return absl::FailedPreconditionError(
          "OnFinalResponseHeader after headers done");
    }
    message_data_.headers.emplace_back(std::string(name), std::string(value));
    return absl::OkStatus();
  }

  absl::Status OnFinalResponseHeadersDone() override {
    message_data_.headers_done = true;
    return absl::OkStatus();
  }

  absl::Status OnBodyChunk(absl::string_view body_chunk) override {
    if (message_data_.body_chunks_done) {
      return absl::FailedPreconditionError(
          "OnBodyChunk after body chunks done");
    }
    message_data_.body_chunks.emplace_back(std::string(body_chunk));
    return absl::OkStatus();
  }

  absl::Status OnBodyChunksDone() override {
    message_data_.body_chunks_done = true;
    return absl::OkStatus();
  }

  absl::Status OnTrailer(absl::string_view name,
                         absl::string_view value) override {
    if (message_data_.trailers_done) {
      return absl::FailedPreconditionError("OnTrailer after trailers done");
    }
    message_data_.trailers.emplace_back(std::string(name), std::string(value));
    return absl::OkStatus();
  }

  absl::Status OnTrailersDone() override {
    message_data_.trailers_done = true;
    return absl::OkStatus();
  }

  const MessageData& GetMessageData() const { return message_data_; }

 private:
  MessageData message_data_;
};

absl::Status ExpectResponseMessageSectionHandler(
    const ResponseMessageSectionTestHandler::MessageData& message_data) {
  const std::vector<std::pair<std::string /*name*/, std::string /*value*/>>
      expected_info1_headers = {{"running", "\"sleep 15\""}};
  const std::vector<std::pair<std::string /*name*/, std::string /*value*/>>
      expected_info2_headers = {
          {"link", "</style.css>; rel=preload; as=style"},
          {"link", "</script.js>; rel=preload; as=script"},
          {"longer_header_value",
           "1111111111111111111111111111111111111111111111111111111111111111"}};
  const std::vector<std::pair<std::string /*name*/, std::string /*value*/>>
      expected_headers = {
          {"date", "Mon, 27 Jul 2009 12:28:53 GMT"},
          {"server", "Apache"},
          {"longer_header_value",
           "1111111111111111111111111111111111111111111111111111111111111111"}};
  const std::vector<std::string> expected_body_chunks = {
      "chunk1", "chunk2", "chunk3",
      "1111111111111111111111111111111111111111111111111111111111111111"};
  const std::vector<std::pair<std::string /*name*/, std::string /*value*/>>
      expected_trailers = {
          {"trailer1", "value1"},
          {"trailer2", "value2"},
          {"longer_trailer_value",
           "1111111111111111111111111111111111111111111111111111111111111111"}};

  if (!message_data.informational_responses_section_done) {
    return absl::FailedPreconditionError("informational responses not done");
  } else if (message_data.informational_responses.size() != 2) {
    return absl::FailedPreconditionError(
        "informational responses size mismatch");
  } else if (message_data.informational_responses[0].status_code != 102) {
    return absl::FailedPreconditionError("info resp 1 status code mismatch");
  } else if (message_data.informational_responses[0].headers !=
             expected_info1_headers) {
    return absl::FailedPreconditionError("info resp 1 headers mismatch");
  } else if (message_data.informational_responses[1].status_code != 103) {
    return absl::FailedPreconditionError("info resp 2 status code mismatch");
  } else if (message_data.informational_responses[1].headers !=
             expected_info2_headers) {
    return absl::FailedPreconditionError("info resp 2 headers mismatch");
  } else if (message_data.final_status_code != 200) {
    return absl::FailedPreconditionError("final status code mismatch");
  } else if (!message_data.headers_done) {
    return absl::FailedPreconditionError("headers not done");
  } else if (message_data.headers != expected_headers) {
    return absl::FailedPreconditionError("headers mismatch");
  } else if (!message_data.body_chunks_done) {
    return absl::FailedPreconditionError("body chunks not done");
  } else if (message_data.body_chunks != expected_body_chunks) {
    return absl::FailedPreconditionError("body chunks mismatch");
  } else if (!message_data.trailers_done) {
    return absl::FailedPreconditionError("trailers not done");
  } else if (message_data.trailers != expected_trailers) {
    return absl::FailedPreconditionError("trailers mismatch");
  } else {
    return absl::OkStatus();
  }
}

TEST(ResponseIndeterminateLengthDecoder, FullResponseDecodingSuccess) {
  std::string response_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kInfoResp1StatusCode, kInfoResp1Headers, kContentTerminator,
                   kInfoResp2StatusCode, kInfoResp2Headers, kContentTerminator,
                   kFinalResponseStatusCode, kFinalResponseHeaders,
                   kContentTerminator, kFinalResponseBody, kContentTerminator,
                   kFinalResponseTrailers, kContentTerminator, kPadding),
      &response_bytes));

  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);
  QUICHE_EXPECT_OK(decoder.Decode(response_bytes, /*end_stream=*/true));

  QUICHE_EXPECT_OK(
      ExpectResponseMessageSectionHandler(handler.GetMessageData()));
}

TEST(ResponseIndeterminateLengthDecoder, BufferedResponseDecodingSuccess) {
  std::string response_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kInfoResp1StatusCode, kInfoResp1Headers, kContentTerminator,
                   kInfoResp2StatusCode, kInfoResp2Headers, kContentTerminator,
                   kFinalResponseStatusCode, kFinalResponseHeaders,
                   kContentTerminator, kFinalResponseBody, kContentTerminator,
                   kFinalResponseTrailers, kContentTerminator, kPadding),
      &response_bytes));

  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);

  // Decode byte by byte to exercise buffering.
  for (uint64_t i = 0; i < response_bytes.size() - 1; i++) {
    QUICHE_EXPECT_OK(decoder.Decode(absl::string_view(&response_bytes[i], 1),
                                    /*end_stream=*/false));
  }
  // Decode the last byte, send end_stream.
  QUICHE_EXPECT_OK(decoder.Decode(
      absl::string_view(&response_bytes[response_bytes.size() - 1], 1),
      /*end_stream=*/true));
  QUICHE_EXPECT_OK(
      ExpectResponseMessageSectionHandler(handler.GetMessageData()));
}

TEST(ResponseIndeterminateLengthDecoder,
     NoInformationalResponseDecodingSuccess) {
  std::string response_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kFinalResponseStatusCode, kFinalResponseHeaders,
                   kContentTerminator, kFinalResponseBody, kContentTerminator,
                   kFinalResponseTrailers, kContentTerminator, kPadding),
      &response_bytes));

  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);

  QUICHE_EXPECT_OK(decoder.Decode(response_bytes, /*end_stream=*/true));

  const ResponseMessageSectionTestHandler::MessageData& message_data =
      handler.GetMessageData();
  EXPECT_TRUE(message_data.informational_responses_section_done);
  EXPECT_TRUE(message_data.informational_responses.empty());
}

TEST(ResponseIndeterminateLengthDecoder,
     OutOfRangeTreatedAsInvalidArgumentWhenEndStream) {
  std::string incomplete_response_bytes = "034066";
  std::string response_bytes;
  EXPECT_TRUE(
      absl::HexStringToBytes(incomplete_response_bytes, &response_bytes));

  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);

  EXPECT_THAT(decoder.Decode(response_bytes, /*end_stream=*/true),
              test::StatusIs(kInvalidArgument));
}

TEST(ResponseIndeterminateLengthDecoder, InvalidFrameFails) {
  std::string response_bytes;
  EXPECT_TRUE(absl::HexStringToBytes("00", &response_bytes));

  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);

  EXPECT_THAT(decoder.Decode(response_bytes, /*end_stream=*/false),
              test::StatusIs(kInvalidArgument));
}

TEST(ResponseIndeterminateLengthDecoder, EmptyFrameFails) {
  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);

  EXPECT_THAT(
      decoder.Decode(/*data=*/"", /*end_stream=*/true),
      test::StatusIs(kInvalidArgument, HasSubstr("Failed to read framing.")));
}

TEST(ResponseIndeterminateLengthDecoder, InvalidPaddingFails) {
  std::string response_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kInfoResp1StatusCode, kInfoResp1Headers, kContentTerminator,
                   kInfoResp2StatusCode, kInfoResp2Headers, kContentTerminator,
                   kFinalResponseStatusCode, kFinalResponseHeaders,
                   kContentTerminator, kFinalResponseBody, kContentTerminator,
                   kFinalResponseTrailers, kContentTerminator, kPadding),
      &response_bytes));

  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);

  QUICHE_EXPECT_OK(decoder.Decode(response_bytes, /*end_stream=*/false));
  EXPECT_THAT(decoder.Decode(/*data=*/"\x01", /*end_stream=*/false),
              test::StatusIs(kInvalidArgument));
}

TEST(ResponseIndeterminateLengthDecoder, InvalidStatusCode) {
  std::string response_bytes;
  // 4063 is "99" as status code.
  EXPECT_TRUE(absl::HexStringToBytes("034063", &response_bytes));

  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);

  EXPECT_THAT(decoder.Decode(response_bytes, /*end_stream=*/false),
              test::StatusIs(kInvalidArgument));
}

TEST(ResponseIndeterminateLengthDecoder, TruncatedTrailers) {
  std::string response_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kInfoResp1StatusCode, kInfoResp1Headers, kContentTerminator,
                   kInfoResp2StatusCode, kInfoResp2Headers, kContentTerminator,
                   kFinalResponseStatusCode, kFinalResponseHeaders,
                   kContentTerminator, kFinalResponseBody, kContentTerminator),
      &response_bytes));

  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);

  QUICHE_EXPECT_OK(decoder.Decode(response_bytes, /*end_stream=*/true));

  const ResponseMessageSectionTestHandler::MessageData& message_data =
      handler.GetMessageData();
  EXPECT_TRUE(message_data.body_chunks_done);
  std::vector<std::string> expected_body_chunks = {
      "chunk1", "chunk2", "chunk3",
      "1111111111111111111111111111111111111111111111111111111111111111"};
  EXPECT_THAT(message_data.body_chunks, ContainerEq(expected_body_chunks));
  EXPECT_TRUE(message_data.trailers_done);
  EXPECT_TRUE(message_data.trailers.empty());
}

TEST(ResponseIndeterminateLengthDecoder, TruncatedBodyAndTrailers) {
  std::string response_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kInfoResp1StatusCode, kInfoResp1Headers, kContentTerminator,
                   kInfoResp2StatusCode, kInfoResp2Headers, kContentTerminator,
                   kFinalResponseStatusCode, kFinalResponseHeaders,
                   kContentTerminator),
      &response_bytes));

  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);

  QUICHE_EXPECT_OK(decoder.Decode(response_bytes, /*end_stream=*/true));

  const ResponseMessageSectionTestHandler::MessageData& message_data =
      handler.GetMessageData();
  EXPECT_TRUE(message_data.body_chunks_done);
  EXPECT_TRUE(message_data.body_chunks.empty());
  EXPECT_TRUE(message_data.trailers_done);
  EXPECT_TRUE(message_data.trailers.empty());
}

TEST(ResponseIndeterminateLengthDecoder, TruncatedTrailersSplitEndStream) {
  std::string response_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kInfoResp1StatusCode, kInfoResp1Headers, kContentTerminator,
                   kInfoResp2StatusCode, kInfoResp2Headers, kContentTerminator,
                   kFinalResponseStatusCode, kFinalResponseHeaders,
                   kContentTerminator, kFinalResponseBody, kContentTerminator),
      &response_bytes));

  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);

  QUICHE_EXPECT_OK(decoder.Decode(response_bytes, /*end_stream=*/false));
  QUICHE_EXPECT_OK(decoder.Decode(/*data=*/"", /*end_stream=*/true));

  const ResponseMessageSectionTestHandler::MessageData& message_data =
      handler.GetMessageData();
  EXPECT_TRUE(message_data.body_chunks_done);
  std::vector<std::string> expected_body_chunks = {
      "chunk1", "chunk2", "chunk3",
      "1111111111111111111111111111111111111111111111111111111111111111"};
  EXPECT_THAT(message_data.body_chunks, ContainerEq(expected_body_chunks));
  EXPECT_TRUE(message_data.trailers_done);
  EXPECT_TRUE(message_data.trailers.empty());
}

TEST(ResponseIndeterminateLengthDecoder,
     TruncatedBodyAndTrailersSplitEndStream) {
  std::string response_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kInfoResp1StatusCode, kInfoResp1Headers, kContentTerminator,
                   kInfoResp2StatusCode, kInfoResp2Headers, kContentTerminator,
                   kFinalResponseStatusCode, kFinalResponseHeaders,
                   kContentTerminator),
      &response_bytes));

  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);

  QUICHE_EXPECT_OK(decoder.Decode(response_bytes, /*end_stream=*/false));
  QUICHE_EXPECT_OK(decoder.Decode(/*data=*/"", /*end_stream=*/true));

  const ResponseMessageSectionTestHandler::MessageData& message_data =
      handler.GetMessageData();
  EXPECT_TRUE(message_data.body_chunks_done);
  EXPECT_TRUE(message_data.body_chunks.empty());
  EXPECT_TRUE(message_data.trailers_done);
  EXPECT_TRUE(message_data.trailers.empty());
}

TEST(ResponseIndeterminateLengthDecoder, InvalidDecodeAfterEndStream) {
  std::string response_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kInfoResp1StatusCode, kContentTerminator),
      &response_bytes));

  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);

  EXPECT_THAT(decoder.Decode(response_bytes, /*end_stream=*/true),
              test::StatusIs(kInvalidArgument));

  EXPECT_THAT(decoder.Decode(response_bytes, /*end_stream=*/false),
              test::StatusIs(kInternal));
}

class MockFailingResponseMessageSectionHandler
    : public BinaryHttpResponse::IndeterminateLengthDecoder::
          MessageSectionHandler {
 public:
  MOCK_METHOD(absl::Status, OnInformationalResponseStatusCode,
              (uint16_t status_code), (override));
  MOCK_METHOD(absl::Status, OnInformationalResponseHeader,
              (absl::string_view name, absl::string_view value), (override));
  MOCK_METHOD(absl::Status, OnInformationalResponseDone, (), (override));
  MOCK_METHOD(absl::Status, OnInformationalResponsesSectionDone, (),
              (override));
  MOCK_METHOD(absl::Status, OnFinalResponseStatusCode, (uint16_t status_code),
              (override));
  MOCK_METHOD(absl::Status, OnFinalResponseHeader,
              (absl::string_view name, absl::string_view value), (override));
  MOCK_METHOD(absl::Status, OnFinalResponseHeadersDone, (), (override));
  MOCK_METHOD(absl::Status, OnBodyChunk, (absl::string_view body_chunk),
              (override));
  MOCK_METHOD(absl::Status, OnBodyChunksDone, (), (override));
  MOCK_METHOD(absl::Status, OnTrailer,
              (absl::string_view name, absl::string_view value), (override));
  MOCK_METHOD(absl::Status, OnTrailersDone, (), (override));
};

std::unique_ptr<MockFailingResponseMessageSectionHandler>
GetMockResponseHandler() {
  auto handler = std::make_unique<MockFailingResponseMessageSectionHandler>();
  ON_CALL(*handler, OnInformationalResponseStatusCode(_))
      .WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnInformationalResponseHeader(_, _))
      .WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnInformationalResponseDone())
      .WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnInformationalResponsesSectionDone())
      .WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnFinalResponseStatusCode(_))
      .WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnFinalResponseHeader(_, _))
      .WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnFinalResponseHeadersDone())
      .WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnBodyChunk(_)).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnBodyChunksDone()).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnTrailer(_, _)).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(*handler, OnTrailersDone()).WillByDefault(Return(absl::OkStatus()));
  return handler;
}

struct FailedMessageSectionHandlerTestParam {
  std::string name;
  std::function<void(MockFailingResponseMessageSectionHandler&,
                     const std::string&)>
      mock_setup;
};

using FailedMessageSectionHandlerTest =
    QuicheTestWithParam<FailedMessageSectionHandlerTestParam>;

TEST_P(FailedMessageSectionHandlerTest, FailsOnMockedError) {
  const FailedMessageSectionHandlerTestParam& param = GetParam();
  std::string response_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kInfoResp1StatusCode, kInfoResp1Headers, kContentTerminator,
                   kInfoResp2StatusCode, kInfoResp2Headers, kContentTerminator,
                   kFinalResponseStatusCode, kFinalResponseHeaders,
                   kContentTerminator, kFinalResponseBody, kContentTerminator,
                   kFinalResponseTrailers, kContentTerminator, kPadding),
      &response_bytes));

  std::unique_ptr<MockFailingResponseMessageSectionHandler> handler =
      GetMockResponseHandler();
  const std::string error_message = "handler error";
  param.mock_setup(*handler, error_message);
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(handler.get());
  EXPECT_THAT(decoder.Decode(response_bytes, /*end_stream=*/true),
              test::StatusIs(kInternal, HasSubstr(error_message)));
}

INSTANTIATE_TEST_SUITE_P(
    FailedMessageSectionHandlerTestInstantiation,
    FailedMessageSectionHandlerTest,
    ValuesIn<FailedMessageSectionHandlerTestParam>(
        {{"InformationalStatusCodeError",
          [](MockFailingResponseMessageSectionHandler& handler,
             const std::string& error_message) {
            EXPECT_CALL(handler, OnInformationalResponseStatusCode(_))
                .WillOnce(Return(absl::InternalError(error_message)));
          }},
         {"InformationalHeaderError",
          [](MockFailingResponseMessageSectionHandler& handler,
             const std::string& error_message) {
            EXPECT_CALL(handler, OnInformationalResponseHeader(_, _))
                .WillOnce(Return(absl::InternalError(error_message)));
          }},
         {"InfoormationalResponseDoneError",
          [](MockFailingResponseMessageSectionHandler& handler,
             const std::string& error_message) {
            EXPECT_CALL(handler, OnInformationalResponseDone())
                .WillOnce(Return(absl::InternalError(error_message)));
          }},
         {"InformationalResponsesSectionDoneError",
          [](MockFailingResponseMessageSectionHandler& handler,
             const std::string& error_message) {
            EXPECT_CALL(handler, OnInformationalResponsesSectionDone())
                .WillOnce(Return(absl::InternalError(error_message)));
          }},
         {"FinalStatusCodeError",
          [](MockFailingResponseMessageSectionHandler& handler,
             const std::string& error_message) {
            EXPECT_CALL(handler, OnFinalResponseStatusCode(_))
                .WillOnce(Return(absl::InternalError(error_message)));
          }},
         {"FinalHeaderError",
          [](MockFailingResponseMessageSectionHandler& handler,
             const std::string& error_message) {
            EXPECT_CALL(handler, OnFinalResponseHeader(_, _))
                .WillOnce(Return(absl::InternalError(error_message)));
          }},
         {"FinalHeadersDoneError",
          [](MockFailingResponseMessageSectionHandler& handler,
             const std::string& error_message) {
            EXPECT_CALL(handler, OnFinalResponseHeadersDone())
                .WillOnce(Return(absl::InternalError(error_message)));
          }},
         {"BodyChunkError",
          [](MockFailingResponseMessageSectionHandler& handler,
             const std::string& error_message) {
            EXPECT_CALL(handler, OnBodyChunk(_))
                .WillOnce(Return(absl::InternalError(error_message)));
          }},
         {"BodyChunksDoneError",
          [](MockFailingResponseMessageSectionHandler& handler,
             const std::string& error_message) {
            EXPECT_CALL(handler, OnBodyChunksDone())
                .WillOnce(Return(absl::InternalError(error_message)));
          }},
         {"TrailerError",
          [](MockFailingResponseMessageSectionHandler& handler,
             const std::string& error_message) {
            EXPECT_CALL(handler, OnTrailer(_, _))
                .WillOnce(Return(absl::InternalError(error_message)));
          }},
         {"TrailersDoneError",
          [](MockFailingResponseMessageSectionHandler& handler,
             const std::string& error_message) {
            EXPECT_CALL(handler, OnTrailersDone())
                .WillOnce(Return(absl::InternalError(error_message)));
          }}}),
    [](const TestParamInfo<FailedMessageSectionHandlerTest::ParamType>& info) {
      return info.param.name;
    });

struct InvalidEndStreamResponseTestCase {
  std::string name;
  std::string response;
};

using InvalidEndStreamResponseTest =
    QuicheTestWithParam<InvalidEndStreamResponseTestCase>;

TEST_P(InvalidEndStreamResponseTest, InvalidEndStreamError) {
  const InvalidEndStreamResponseTestCase& test_case = GetParam();
  ResponseMessageSectionTestHandler handler;
  BinaryHttpResponse::IndeterminateLengthDecoder decoder(&handler);
  std::string response_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(test_case.response, &response_bytes));
  EXPECT_THAT(decoder.Decode(response_bytes, /*end_stream=*/true),
              test::StatusIs(kInvalidArgument));
}

INSTANTIATE_TEST_SUITE_P(
    InvalidEndStreamResponseTestInstantiation, InvalidEndStreamResponseTest,
    ValuesIn<InvalidEndStreamResponseTestCase>({
        {"incomplete_info_header",
         absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                      kInfoResp1StatusCode)},
        {"incomplete_final_header",
         absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                      kFinalResponseStatusCode)},
        {"incomplete_body",
         absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                      kFinalResponseStatusCode, kContentTerminator,
                      kFinalResponseBody)},
        {"incomplete_trailer",
         absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                      kFinalResponseStatusCode, kContentTerminator,
                      kContentTerminator, kFinalResponseTrailers)},
    }),
    [](const TestParamInfo<InvalidEndStreamResponseTest::ParamType>& info) {
      return info.param.name;
    });

TEST(ResponseIndeterminateLengthEncoder, WithInformationalResponses) {
  std::string expected;
  ASSERT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kInfoResp1StatusCode, kInfoResp1Headers, kContentTerminator,
                   kInfoResp2StatusCode, kInfoResp2Headers, kContentTerminator,
                   kFinalResponseStatusCode, kFinalResponseHeaders,
                   kContentTerminator, kFinalResponseBody, kContentTerminator,
                   kFinalResponseTrailers, kContentTerminator),
      &expected));

  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  ResponseIndeterminateLengthEncoderTestData test_data;
  std::string encoded_data;

  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeInformationalResponse(102,
                                          test_data.informationalResponse1);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;
  status_or_encoded_data = encoder.EncodeInformationalResponse(
      103, test_data.informationalResponse2);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;

  status_or_encoded_data = encoder.EncodeHeaders(200, test_data.headers);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;

  status_or_encoded_data =
      encoder.EncodeBodyChunks(test_data.body_chunks, true);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;

  status_or_encoded_data = encoder.EncodeTrailers(test_data.trailers);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;

  EXPECT_EQ(encoded_data, expected);
}

TEST(ResponseIndeterminateLengthEncoder, NoInformationalResponses) {
  std::string expected;
  ASSERT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kFinalResponseStatusCode, kFinalResponseHeaders,
                   kContentTerminator, kFinalResponseBody, kContentTerminator,
                   kFinalResponseTrailers, kContentTerminator),
      &expected));

  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  ResponseIndeterminateLengthEncoderTestData test_data;
  std::string encoded_data;

  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeHeaders(200, test_data.headers);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;

  status_or_encoded_data =
      encoder.EncodeBodyChunks(test_data.body_chunks, true);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;

  status_or_encoded_data = encoder.EncodeTrailers(test_data.trailers);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;

  EXPECT_EQ(encoded_data, expected);
}

TEST(ResponseIndeterminateLengthEncoder, EncodingChunksMultipleTimes) {
  std::string expected;
  ASSERT_TRUE(absl::HexStringToBytes(
      absl::StrCat(kIndeterminateLengthResponseFramingIndicator,
                   kFinalResponseStatusCode, kFinalResponseHeaders,
                   kContentTerminator, kFinalResponseBody, kContentTerminator,
                   kFinalResponseTrailers, kContentTerminator),
      &expected));

  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  ResponseIndeterminateLengthEncoderTestData test_data;
  std::string encoded_data;

  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeHeaders(200, test_data.headers);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;

  status_or_encoded_data =
      encoder.EncodeBodyChunks({test_data.body_chunks[0]}, false);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;
  status_or_encoded_data =
      encoder.EncodeBodyChunks({test_data.body_chunks[1]}, false);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;
  status_or_encoded_data =
      encoder.EncodeBodyChunks({test_data.body_chunks[2]}, false);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;
  status_or_encoded_data =
      encoder.EncodeBodyChunks({test_data.body_chunks[3]}, false);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;
  status_or_encoded_data = encoder.EncodeBodyChunks({}, true);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;

  status_or_encoded_data = encoder.EncodeTrailers(test_data.trailers);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  encoded_data += *status_or_encoded_data;
  EXPECT_EQ(encoded_data, expected);
}

TEST(ResponseIndeterminateLengthEncoder, EncodingWrongStatusCodes) {
  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeInformationalResponse(99, {});
  EXPECT_THAT(status_or_encoded_data, test::StatusIs(kInvalidArgument));

  encoder = BinaryHttpResponse::IndeterminateLengthEncoder();
  status_or_encoded_data = encoder.EncodeInformationalResponse(200, {});
  EXPECT_THAT(status_or_encoded_data, test::StatusIs(kInvalidArgument));

  encoder = BinaryHttpResponse::IndeterminateLengthEncoder();
  status_or_encoded_data = encoder.EncodeHeaders(199, {});
  EXPECT_THAT(status_or_encoded_data, test::StatusIs(kInvalidArgument));

  encoder = BinaryHttpResponse::IndeterminateLengthEncoder();
  status_or_encoded_data = encoder.EncodeHeaders(600, {});
  EXPECT_THAT(status_or_encoded_data, test::StatusIs(kInvalidArgument));
}

TEST(ResponseIndeterminateLengthEncoder, OutOfOrderBodyChunks) {
  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeBodyChunks({}, true);
  EXPECT_THAT(status_or_encoded_data, test::StatusIs(kInvalidArgument));
}

TEST(ResponseIndeterminateLengthEncoder, OutOfOrderTrailers) {
  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeTrailers({});
  EXPECT_THAT(status_or_encoded_data, test::StatusIs(kInvalidArgument));
}

TEST(ResponseIndeterminateLengthEncoder, OutOfOrderInformationalResponse) {
  // Cannot encode informational responses after headers.
  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeHeaders(200, {});
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeInformationalResponse(102, {});
  EXPECT_THAT(status_or_encoded_data, test::StatusIs(kInvalidArgument));
}

TEST(ResponseIndeterminateLengthEncoder, NoFinalChunk) {
  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeHeaders(200, {});
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeBodyChunks({"foobar"}, false);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeBodyChunks({}, true);
  QUICHE_EXPECT_OK(status_or_encoded_data);
}

TEST(ResponseIndeterminateLengthEncoder, EmptyFinalChunk) {
  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeHeaders(200, {});
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeBodyChunks({"foobar"}, false);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeBodyChunks({""}, true);
  QUICHE_EXPECT_OK(status_or_encoded_data);
}

TEST(ResponseIndeterminateLengthEncoder, MustNotEncodeChunksAfterChunksDone) {
  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeHeaders(200, {});
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeBodyChunks({}, true);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeBodyChunks({}, true);
  EXPECT_THAT(status_or_encoded_data, test::StatusIs(kInvalidArgument));
}

TEST(ResponseIndeterminateLengthEncoder, MustNotEncodeNonFinalNoChunks) {
  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeHeaders(200, {});
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeBodyChunks({}, false);
  EXPECT_THAT(status_or_encoded_data, test::StatusIs(kInvalidArgument));
}

TEST(ResponseIndeterminateLengthEncoder, MustNotEncodeNonFinalEmptyChunk) {
  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeHeaders(200, {});
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeBodyChunks({""}, false);
  EXPECT_THAT(status_or_encoded_data, test::StatusIs(kInvalidArgument));
}

TEST(ResponseIndeterminateLengthEncoder, MustNotEncodeHeadersTwice) {
  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeHeaders(200, {});
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeHeaders(200, {});
  EXPECT_THAT(status_or_encoded_data, test::StatusIs(kInvalidArgument));
}

TEST(ResponseIndeterminateLengthEncoder, MustNotEncodeTrailersTwice) {
  BinaryHttpResponse::IndeterminateLengthEncoder encoder;
  absl::StatusOr<std::string> status_or_encoded_data =
      encoder.EncodeHeaders(200, {});
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeBodyChunks({}, true);
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeTrailers({});
  QUICHE_EXPECT_OK(status_or_encoded_data);
  status_or_encoded_data = encoder.EncodeTrailers({});
  EXPECT_THAT(status_or_encoded_data, test::StatusIs(kInvalidArgument));
}

TEST(IndeterminateLengthDecoder, TruncatedTrailers) {
  RequestMessageSectionTestHandler handler;
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(&handler);
  std::string request_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(
          k2ByteFramingIndicator, kIndeterminateLengthEncodedRequestControlData,
          kIndeterminateLengthEncodedRequestHeaders, k8ByteContentTerminator,
          kIndeterminateLengthEncodedRequestBodyChunks,
          k4ByteContentTerminator),
      &request_bytes));

  QUICHE_EXPECT_OK(decoder.Decode(request_bytes, /*end_stream=*/true));
  auto message_data = handler.GetMessageData();
  EXPECT_TRUE(message_data.body_chunks_done_);
  std::vector<std::string> expected_body_chunks = {"chunk1", "chunk2",
                                                   "chunk3"};
  EXPECT_THAT(message_data.body_chunks_, ContainerEq(expected_body_chunks));
  QUICHE_EXPECT_OK(ExpectTruncatedTrailerSection(message_data));
}

TEST(IndeterminateLengthDecoder, TruncatedTrailersSplitEndStream) {
  RequestMessageSectionTestHandler handler;
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(&handler);
  std::string request_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(
          k2ByteFramingIndicator, kIndeterminateLengthEncodedRequestControlData,
          kIndeterminateLengthEncodedRequestHeaders, k8ByteContentTerminator,
          kIndeterminateLengthEncodedRequestBodyChunks,
          k4ByteContentTerminator),
      &request_bytes));

  QUICHE_EXPECT_OK(decoder.Decode(request_bytes, /*end_stream=*/false));
  // Send `end_stream` with no data.
  QUICHE_EXPECT_OK(decoder.Decode("", /*end_stream=*/true));
  auto message_data = handler.GetMessageData();
  EXPECT_TRUE(message_data.body_chunks_done_);
  std::vector<std::string> expected_body_chunks = {"chunk1", "chunk2",
                                                   "chunk3"};
  EXPECT_THAT(message_data.body_chunks_, ContainerEq(expected_body_chunks));
  QUICHE_EXPECT_OK(ExpectTruncatedTrailerSection(message_data));
}

TEST(IndeterminateLengthDecoder, InvalidDecodeAfterEndStream) {
  std::string request_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(
      absl::StrCat(
          k2ByteFramingIndicator, kIndeterminateLengthEncodedRequestControlData,
          kIndeterminateLengthEncodedRequestHeaders, k8ByteContentTerminator),
      &request_bytes));
  RequestMessageSectionTestHandler handler;
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(&handler);
  QUICHE_EXPECT_OK(decoder.Decode(request_bytes, /*end_stream=*/true));
  absl::Status status = decoder.Decode(request_bytes, /*end_stream=*/false);
  EXPECT_THAT(status, test::StatusIs(kInternal));
}

struct InvalidEndStreamTestCase {
  std::string name;
  std::string request;
};

using InvalidEndStreamTest = QuicheTestWithParam<InvalidEndStreamTestCase>;

TEST_P(InvalidEndStreamTest, InvalidEndStreamError) {
  const InvalidEndStreamTestCase& test_case = GetParam();
  RequestMessageSectionTestHandler handler;
  BinaryHttpRequest::IndeterminateLengthDecoder decoder(&handler);
  std::string request_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(test_case.request, &request_bytes));
  absl::Status status = decoder.Decode(request_bytes, /*end_stream=*/true);
  EXPECT_THAT(status, test::StatusIs(kInvalidArgument));
}

INSTANTIATE_TEST_SUITE_P(
    InvalidEndStreamTestInstantiation, InvalidEndStreamTest,
    ValuesIn<InvalidEndStreamTestCase>({
        {
            "headers_not_terminated",
            "02"                      // Indeterminate length request frame
            "04504F5354"              // :method = POST
            "056874747073"            // :scheme = https
            "0A676F6F676C652E636F6D"  // :authority = "google.com"
            "062F68656C6C6F"          // :path = /hello
            "0A757365722D6167656E74"  // user-agent
        },
        {"body_not_terminated",
         absl::StrCat(kIndeterminateLengthEncodedRequestHeaders,
                      "066368756E6B31"  // chunk1
                      )},
        {"trailers_not_terminated",
         absl::StrCat(kIndeterminateLengthEncodedRequestHeaders,
                      kIndeterminateLengthEncodedRequestBodyChunks,
                      "08747261696C657231"  // trailer1
                      "0676616C756531"      // value1
                      "08747261696C657232"  // trailer2
                      )},
    }),
    [](const TestParamInfo<InvalidEndStreamTest::ParamType>& info) {
      return info.param.name;
    });

TEST(BinaryHttpResponse, EncodeNoBody) {
  /*
    HTTP/1.1 404 Not Found
    Server: Apache
  */
  BinaryHttpResponse response(404);
  response.AddHeaderField({"Server", "Apache"});
  /*
    0141940e 06736572 76657206 41706163  .A...server.Apac
    686500                               he..
  */
  const uint32_t expected_words[] = {0x0141940e, 0x06736572, 0x76657206,
                                     0x41706163, 0x68650000};
  std::string expected;
  for (const auto& word : expected_words) {
    expected += WordToBytes(word);
  }
  // Remove padding.
  expected.resize(expected.size() - 1);
  const auto result = response.Serialize();
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(*result, expected);
  EXPECT_THAT(
      response.DebugString(),
      StrEq("BinaryHttpResponse(404){BinaryHttpMessage{Headers{Field{server="
            "Apache}}Body{}}}"));
}

TEST(BinaryHttpResponse, DecodeNoBody) {
  absl::string_view known_length_request =
      "80000001"  // 4-byte framing. The framing indicator is normally encoded
                  // using a single byte but we intentionally use a
                  // multiple-byte value here to test the decoding logic.
      "4194"      // status 404
      "0E"        // field section length
      "06736572766572"  // server
      "06417061636865"  // Apache
      "000000";         // padding

  std::string data;
  ASSERT_TRUE(absl::HexStringToBytes(known_length_request, &data));
  /*
    HTTP/1.1 404 Not Found
    Server: Apache
  */

  const auto response_so = BinaryHttpResponse::Create(data);
  ASSERT_TRUE(response_so.ok());
  const BinaryHttpResponse response = *response_so;
  ASSERT_EQ(response.status_code(), 404);
  std::vector<BinaryHttpMessage::Field> expected_fields = {
      {"server", "Apache"}};
  ASSERT_THAT(response.GetHeaderFields(), ContainerEq(expected_fields));
  ASSERT_EQ(response.body(), "");
  ASSERT_TRUE(response.informational_responses().empty());
  EXPECT_THAT(
      response.DebugString(),
      StrEq("BinaryHttpResponse(404){BinaryHttpMessage{Headers{Field{server="
            "Apache}}Body{}}}"));
}

TEST(BinaryHttpResponse, EncodeBody) {
  /*
    HTTP/1.1 200 OK
    Server: Apache

    Hello, world!
  */
  BinaryHttpResponse response(200);
  response.AddHeaderField({"Server", "Apache"});
  response.set_body("Hello, world!\r\n");
  /*
    0140c80e 06736572 76657206 41706163  .@...server.Apac
    68650f48 656c6c6f 2c20776f 726c6421  he.Hello, world!
    0d0a                                 ....
  */
  const uint32_t expected_words[] = {0x0140c80e, 0x06736572, 0x76657206,
                                     0x41706163, 0x68650f48, 0x656c6c6f,
                                     0x2c20776f, 0x726c6421, 0x0d0a0000};
  std::string expected;
  for (const auto& word : expected_words) {
    expected += WordToBytes(word);
  }
  // Remove padding.
  expected.resize(expected.size() - 2);

  const auto result = response.Serialize();
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(*result, expected);
  EXPECT_THAT(
      response.DebugString(),
      StrEq("BinaryHttpResponse(200){BinaryHttpMessage{Headers{Field{server="
            "Apache}}Body{Hello, world!\r\n}}}"));
}

TEST(BinaryHttpResponse, DecodeBody) {
  /*
    HTTP/1.1 200 OK

    Hello, world!
  */
  const uint32_t words[] = {0x0140c80e, 0x06736572, 0x76657206,
                            0x41706163, 0x68650f48, 0x656c6c6f,
                            0x2c20776f, 0x726c6421, 0x0d0a0000};
  std::string data;
  for (const auto& word : words) {
    data += WordToBytes(word);
  }
  const auto response_so = BinaryHttpResponse::Create(data);
  ASSERT_TRUE(response_so.ok());
  const BinaryHttpResponse response = *response_so;
  ASSERT_EQ(response.status_code(), 200);
  std::vector<BinaryHttpMessage::Field> expected_fields = {
      {"server", "Apache"}};
  ASSERT_THAT(response.GetHeaderFields(), ContainerEq(expected_fields));
  ASSERT_EQ(response.body(), "Hello, world!\r\n");
  ASSERT_TRUE(response.informational_responses().empty());
  EXPECT_THAT(
      response.DebugString(),
      StrEq("BinaryHttpResponse(200){BinaryHttpMessage{Headers{Field{server="
            "Apache}}Body{Hello, world!\r\n}}}"));
}

TEST(BHttpResponse, AddBadInformationalResponseCode) {
  BinaryHttpResponse response(200);
  ASSERT_FALSE(response.AddInformationalResponse(50, {}).ok());
  ASSERT_FALSE(response.AddInformationalResponse(300, {}).ok());
}

TEST(BinaryHttpResponse, EncodeMultiInformationalWithBody) {
  /*
    HTTP/1.1 102 Processing
    Running: "sleep 15"

    HTTP/1.1 103 Early Hints
    Link: </style.css>; rel=preload; as=style
    Link: </script.js>; rel=preload; as=script

    HTTP/1.1 200 OK
    Date: Mon, 27 Jul 2009 12:28:53 GMT
    Server: Apache
    Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT
    ETag: "34aa387-d-1568eb00"
    Accept-Ranges: bytes
    Content-Length: 51
    Vary: Accept-Encoding
    Content-Type: text/plain

    Hello World! My content includes a trailing CRLF.
  */
  BinaryHttpResponse response(200);
  response.AddHeaderField({"Date", "Mon, 27 Jul 2009 12:28:53 GMT"})
      ->AddHeaderField({"Server", "Apache"})
      ->AddHeaderField({"Last-Modified", "Wed, 22 Jul 2009 19:15:56 GMT"})
      ->AddHeaderField({"ETag", "\"34aa387-d-1568eb00\""})
      ->AddHeaderField({"Accept-Ranges", "bytes"})
      ->AddHeaderField({"Content-Length", "51"})
      ->AddHeaderField({"Vary", "Accept-Encoding"})
      ->AddHeaderField({"Content-Type", "text/plain"});
  response.set_body("Hello World! My content includes a trailing CRLF.\r\n");
  ASSERT_TRUE(
      response.AddInformationalResponse(102, {{"Running", "\"sleep 15\""}})
          .ok());
  ASSERT_TRUE(response
                  .AddInformationalResponse(
                      103, {{"Link", "</style.css>; rel=preload; as=style"},
                            {"Link", "</script.js>; rel=preload; as=script"}})
                  .ok());

  /*
      01406613 0772756e 6e696e67 0a22736c  .@f..running."sl
      65657020 31352240 67405304 6c696e6b  eep 15"@g@S.link
      233c2f73 74796c65 2e637373 3e3b2072  #</style.css>; r
      656c3d70 72656c6f 61643b20 61733d73  el=preload; as=s
      74796c65 046c696e 6b243c2f 73637269  tyle.link$</scri
      70742e6a 733e3b20 72656c3d 7072656c  pt.js>; rel=prel
      6f61643b 2061733d 73637269 707440c8  oad; as=script@.
      40ca0464 6174651d 4d6f6e2c 20323720  @..date.Mon, 27
      4a756c20 32303039 2031323a 32383a35  Jul 2009 12:28:5
      3320474d 54067365 72766572 06417061  3 GMT.server.Apa
      6368650d 6c617374 2d6d6f64 69666965  che.last-modifie
      641d5765 642c2032 32204a75 6c203230  d.Wed, 22 Jul 20
      30392031 393a3135 3a353620 474d5404  09 19:15:56 GMT.
      65746167 14223334 61613338 372d642d  etag."34aa387-d-
      31353638 65623030 220d6163 63657074  1568eb00".accept
      2d72616e 67657305 62797465 730e636f  -ranges.bytes.co
      6e74656e 742d6c65 6e677468 02353104  ntent-length.51.
      76617279 0f416363 6570742d 456e636f  vary.Accept-Enco
      64696e67 0c636f6e 74656e74 2d747970  ding.content-typ
      650a7465 78742f70 6c61696e 3348656c  e.text/plain3Hel
      6c6f2057 6f726c64 21204d79 20636f6e  lo World! My con
      74656e74 20696e63 6c756465 73206120  tent includes a
      74726169 6c696e67 2043524c 462e0d0a  trailing CRLF...
  */
  const uint32_t expected_words[] = {
      0x01406613, 0x0772756e, 0x6e696e67, 0x0a22736c, 0x65657020, 0x31352240,
      0x67405304, 0x6c696e6b, 0x233c2f73, 0x74796c65, 0x2e637373, 0x3e3b2072,
      0x656c3d70, 0x72656c6f, 0x61643b20, 0x61733d73, 0x74796c65, 0x046c696e,
      0x6b243c2f, 0x73637269, 0x70742e6a, 0x733e3b20, 0x72656c3d, 0x7072656c,
      0x6f61643b, 0x2061733d, 0x73637269, 0x707440c8, 0x40ca0464, 0x6174651d,
      0x4d6f6e2c, 0x20323720, 0x4a756c20, 0x32303039, 0x2031323a, 0x32383a35,
      0x3320474d, 0x54067365, 0x72766572, 0x06417061, 0x6368650d, 0x6c617374,
      0x2d6d6f64, 0x69666965, 0x641d5765, 0x642c2032, 0x32204a75, 0x6c203230,
      0x30392031, 0x393a3135, 0x3a353620, 0x474d5404, 0x65746167, 0x14223334,
      0x61613338, 0x372d642d, 0x31353638, 0x65623030, 0x220d6163, 0x63657074,
      0x2d72616e, 0x67657305, 0x62797465, 0x730e636f, 0x6e74656e, 0x742d6c65,
      0x6e677468, 0x02353104, 0x76617279, 0x0f416363, 0x6570742d, 0x456e636f,
      0x64696e67, 0x0c636f6e, 0x74656e74, 0x2d747970, 0x650a7465, 0x78742f70,
      0x6c61696e, 0x3348656c, 0x6c6f2057, 0x6f726c64, 0x21204d79, 0x20636f6e,
      0x74656e74, 0x20696e63, 0x6c756465, 0x73206120, 0x74726169, 0x6c696e67,
      0x2043524c, 0x462e0d0a};
  std::string expected;
  for (const auto& word : expected_words) {
    expected += WordToBytes(word);
  }
  const auto result = response.Serialize();
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(*result, expected);
  EXPECT_THAT(
      response.DebugString(),
      StrEq(
          "BinaryHttpResponse(200){BinaryHttpMessage{Headers{Field{date=Mon, "
          "27 Jul 2009 12:28:53 "
          "GMT};Field{server=Apache};Field{last-modified=Wed, 22 Jul 2009 "
          "19:15:56 "
          "GMT};Field{etag=\"34aa387-d-1568eb00\"};Field{accept-ranges=bytes};"
          "Field{"
          "content-length=51};Field{vary=Accept-Encoding};Field{content-type="
          "text/plain}}Body{Hello World! My content includes a trailing "
          "CRLF.\r\n}}InformationalResponse{Field{running=\"sleep "
          "15\"}};InformationalResponse{Field{link=</style.css>; rel=preload; "
          "as=style};Field{link=</script.js>; rel=preload; as=script}}}"));
  QUICHE_EXPECT_OK(TestPrintTo(response));
}

TEST(BinaryHttpResponse, DecodeMultiInformationalWithBody) {
  /*
    HTTP/1.1 102 Processing
    Running: "sleep 15"

    HTTP/1.1 103 Early Hints
    Link: </style.css>; rel=preload; as=style
    Link: </script.js>; rel=preload; as=script

    HTTP/1.1 200 OK
    Date: Mon, 27 Jul 2009 12:28:53 GMT
    Server: Apache
    Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT
    ETag: "34aa387-d-1568eb00"
    Accept-Ranges: bytes
    Content-Length: 51
    Vary: Accept-Encoding
    Content-Type: text/plain

    Hello World! My content includes a trailing CRLF.
  */
  const uint32_t words[] = {
      0x01406613, 0x0772756e, 0x6e696e67, 0x0a22736c, 0x65657020, 0x31352240,
      0x67405304, 0x6c696e6b, 0x233c2f73, 0x74796c65, 0x2e637373, 0x3e3b2072,
      0x656c3d70, 0x72656c6f, 0x61643b20, 0x61733d73, 0x74796c65, 0x046c696e,
      0x6b243c2f, 0x73637269, 0x70742e6a, 0x733e3b20, 0x72656c3d, 0x7072656c,
      0x6f61643b, 0x2061733d, 0x73637269, 0x707440c8, 0x40ca0464, 0x6174651d,
      0x4d6f6e2c, 0x20323720, 0x4a756c20, 0x32303039, 0x2031323a, 0x32383a35,
      0x3320474d, 0x54067365, 0x72766572, 0x06417061, 0x6368650d, 0x6c617374,
      0x2d6d6f64, 0x69666965, 0x641d5765, 0x642c2032, 0x32204a75, 0x6c203230,
      0x30392031, 0x393a3135, 0x3a353620, 0x474d5404, 0x65746167, 0x14223334,
      0x61613338, 0x372d642d, 0x31353638, 0x65623030, 0x220d6163, 0x63657074,
      0x2d72616e, 0x67657305, 0x62797465, 0x730e636f, 0x6e74656e, 0x742d6c65,
      0x6e677468, 0x02353104, 0x76617279, 0x0f416363, 0x6570742d, 0x456e636f,
      0x64696e67, 0x0c636f6e, 0x74656e74, 0x2d747970, 0x650a7465, 0x78742f70,
      0x6c61696e, 0x3348656c, 0x6c6f2057, 0x6f726c64, 0x21204d79, 0x20636f6e,
      0x74656e74, 0x20696e63, 0x6c756465, 0x73206120, 0x74726169, 0x6c696e67,
      0x2043524c, 0x462e0d0a, 0x00000000};
  std::string data;
  for (const auto& word : words) {
    data += WordToBytes(word);
  }
  const auto response_so = BinaryHttpResponse::Create(data);
  ASSERT_TRUE(response_so.ok());
  const BinaryHttpResponse response = *response_so;
  std::vector<BinaryHttpMessage::Field> expected_fields = {
      {"date", "Mon, 27 Jul 2009 12:28:53 GMT"},
      {"server", "Apache"},
      {"last-modified", "Wed, 22 Jul 2009 19:15:56 GMT"},
      {"etag", "\"34aa387-d-1568eb00\""},
      {"accept-ranges", "bytes"},
      {"content-length", "51"},
      {"vary", "Accept-Encoding"},
      {"content-type", "text/plain"}};

  ASSERT_THAT(response.GetHeaderFields(), ContainerEq(expected_fields));
  ASSERT_EQ(response.body(),
            "Hello World! My content includes a trailing CRLF.\r\n");
  std::vector<BinaryHttpMessage::Field> header102 = {
      {"running", "\"sleep 15\""}};
  std::vector<BinaryHttpMessage::Field> header103 = {
      {"link", "</style.css>; rel=preload; as=style"},
      {"link", "</script.js>; rel=preload; as=script"}};
  std::vector<BinaryHttpResponse::InformationalResponse> expected_control = {
      {102, header102}, {103, header103}};
  ASSERT_THAT(response.informational_responses(),
              ContainerEq(expected_control));
  EXPECT_THAT(
      response.DebugString(),
      StrEq(
          "BinaryHttpResponse(200){BinaryHttpMessage{Headers{Field{date=Mon, "
          "27 Jul 2009 12:28:53 "
          "GMT};Field{server=Apache};Field{last-modified=Wed, 22 Jul 2009 "
          "19:15:56 "
          "GMT};Field{etag=\"34aa387-d-1568eb00\"};Field{accept-ranges=bytes};"
          "Field{"
          "content-length=51};Field{vary=Accept-Encoding};Field{content-type="
          "text/plain}}Body{Hello World! My content includes a trailing "
          "CRLF.\r\n}}InformationalResponse{Field{running=\"sleep "
          "15\"}};InformationalResponse{Field{link=</style.css>; rel=preload; "
          "as=style};Field{link=</script.js>; rel=preload; as=script}}}"));
  QUICHE_EXPECT_OK(TestPrintTo(response));
}

TEST(BinaryHttpMessage, SwapBody) {
  BinaryHttpRequest request({});
  request.set_body("hello, world!");
  std::string other = "goodbye, world!";
  request.swap_body(other);
  EXPECT_EQ(request.body(), "goodbye, world!");
  EXPECT_EQ(other, "hello, world!");
}

TEST(BinaryHttpResponse, Equality) {
  BinaryHttpResponse response(200);
  response.AddHeaderField({"Server", "Apache"})->set_body("Hello, world!\r\n");
  ASSERT_TRUE(
      response.AddInformationalResponse(102, {{"Running", "\"sleep 15\""}})
          .ok());

  BinaryHttpResponse same(200);
  same.AddHeaderField({"Server", "Apache"})->set_body("Hello, world!\r\n");
  ASSERT_TRUE(
      same.AddInformationalResponse(102, {{"Running", "\"sleep 15\""}}).ok());
  ASSERT_EQ(response, same);
}

TEST(BinaryHttpResponse, Inequality) {
  BinaryHttpResponse response(200);
  response.AddHeaderField({"Server", "Apache"})->set_body("Hello, world!\r\n");
  ASSERT_TRUE(
      response.AddInformationalResponse(102, {{"Running", "\"sleep 15\""}})
          .ok());

  BinaryHttpResponse different_status(201);
  different_status.AddHeaderField({"Server", "Apache"})
      ->set_body("Hello, world!\r\n");
  EXPECT_TRUE(different_status
                  .AddInformationalResponse(102, {{"Running", "\"sleep 15\""}})
                  .ok());
  EXPECT_NE(response, different_status);

  BinaryHttpResponse different_header(200);
  different_header.AddHeaderField({"Server", "python3"})
      ->set_body("Hello, world!\r\n");
  EXPECT_TRUE(different_header
                  .AddInformationalResponse(102, {{"Running", "\"sleep 15\""}})
                  .ok());
  EXPECT_NE(response, different_header);

  BinaryHttpResponse no_header(200);
  no_header.set_body("Hello, world!\r\n");
  EXPECT_TRUE(
      no_header.AddInformationalResponse(102, {{"Running", "\"sleep 15\""}})
          .ok());
  EXPECT_NE(response, no_header);

  BinaryHttpResponse different_body(200);
  different_body.AddHeaderField({"Server", "Apache"})
      ->set_body("Goodbye, world!\r\n");
  EXPECT_TRUE(different_body
                  .AddInformationalResponse(102, {{"Running", "\"sleep 15\""}})
                  .ok());
  EXPECT_NE(response, different_body);

  BinaryHttpResponse no_body(200);
  no_body.AddHeaderField({"Server", "Apache"});
  EXPECT_TRUE(
      no_body.AddInformationalResponse(102, {{"Running", "\"sleep 15\""}})
          .ok());
  EXPECT_NE(response, no_body);

  BinaryHttpResponse different_informational(200);
  different_informational.AddHeaderField({"Server", "Apache"})
      ->set_body("Hello, world!\r\n");
  EXPECT_TRUE(different_informational
                  .AddInformationalResponse(198, {{"Running", "\"sleep 15\""}})
                  .ok());
  EXPECT_NE(response, different_informational);

  BinaryHttpResponse no_informational(200);
  no_informational.AddHeaderField({"Server", "Apache"})
      ->set_body("Hello, world!\r\n");
  EXPECT_NE(response, no_informational);
}

MATCHER_P(HasEqPayload, value, "Payloads of messages are equivalent.") {
  return arg.IsPayloadEqual(value);
}

template <typename T>
absl::Status TestPadding(T& message) {
  const auto data_so = message.Serialize();
  if (!data_so.ok()) {
    return data_so.status();
  }
  auto data = *data_so;
  if (data.size() != message.EncodedSize()) {
    return absl::FailedPreconditionError("Incorrect size");
  }
  message.set_num_padding_bytes(10);
  const auto padded_data_so = message.Serialize();
  if (!padded_data_so.ok()) {
    return padded_data_so.status();
  }
  const auto padded_data = *padded_data_so;
  if (padded_data.size() != message.EncodedSize()) {
    return absl::FailedPreconditionError("Incorrect padded size");
  }
  // Check padding size output.
  if (data.size() + 10 != padded_data.size()) {
    return absl::FailedPreconditionError("Padding size mismatch");
  }
  // Check for valid null byte padding output
  data.resize(data.size() + 10);
  if (data != padded_data) {
    return absl::FailedPreconditionError("Padded data mismatch");
  }
  // Deserialize padded and not padded, and verify they are the same.
  const auto deserialized_padded_message_so = T::Create(data);
  if (!deserialized_padded_message_so.ok()) {
    return deserialized_padded_message_so.status();
  }
  const auto deserialized_padded_message = *deserialized_padded_message_so;
  if (!(deserialized_padded_message == message)) {
    return absl::FailedPreconditionError(
        "Deserialized padded message != message");
  }
  if (deserialized_padded_message.num_padding_bytes() != size_t(10)) {
    return absl::FailedPreconditionError(
        "Padding bytes mismatch in deserialized");
  }
  // Invalid padding
  data[data.size() - 1] = 'a';
  const auto bad_so = T::Create(data);
  if (bad_so.ok()) {
    return absl::FailedPreconditionError(
        "Expected bad status for invalid padding");
  }
  // Check that padding does not impact equality.
  data.resize(data.size() - 10);
  const auto deserialized_message_so = T::Create(data);
  if (!deserialized_message_so.ok()) {
    return deserialized_message_so.status();
  }
  const auto deserialized_message = *deserialized_message_so;
  if (deserialized_message.num_padding_bytes() != size_t(0)) {
    return absl::FailedPreconditionError("Num padding bytes should be 0");
  }
  // Confirm that the message payloads are equal, but not fully equivalent due
  // to padding.
  if (!deserialized_message.IsPayloadEqual(deserialized_padded_message)) {
    return absl::FailedPreconditionError("IsPayloadEqual failed");
  }
  if (deserialized_message == deserialized_padded_message) {
    return absl::FailedPreconditionError(
        "deserialized_message == deserialized_padded_message");
  }
  return absl::OkStatus();
}

TEST(BinaryHttpRequest, Padding) {
  /*
    GET /hello.txt HTTP/1.1
    User-Agent: curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3
    Host: www.example.com
    Accept-Language: en, mi
  */
  BinaryHttpRequest request({"GET", "https", "", "/hello.txt"});
  request
      .AddHeaderField({"User-Agent",
                       "curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3"})
      ->AddHeaderField({"Host", "www.example.com"})
      ->AddHeaderField({"Accept-Language", "en, mi"});
  QUICHE_EXPECT_OK(TestPadding(request));
}

TEST(BinaryHttpResponse, Padding) {
  /*
    HTTP/1.1 200 OK
    Server: Apache

    Hello, world!
  */
  BinaryHttpResponse response(200);
  response.AddHeaderField({"Server", "Apache"});
  response.set_body("Hello, world!\r\n");
  QUICHE_EXPECT_OK(TestPadding(response));
}

}  // namespace
}  // namespace quiche
