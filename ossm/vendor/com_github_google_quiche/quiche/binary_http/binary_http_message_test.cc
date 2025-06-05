#include "quiche/binary_http/binary_http_message.h"

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "quiche/common/platform/api/quiche_test.h"

using ::testing::ContainerEq;
using ::testing::FieldsAre;
using ::testing::StrEq;

namespace quiche {
namespace {

std::string WordToBytes(uint32_t word) {
  return std::string({static_cast<char>(word >> 24),
                      static_cast<char>(word >> 16),
                      static_cast<char>(word >> 8), static_cast<char>(word)});
}

template <class T>
void TestPrintTo(const T& resp) {
  std::ostringstream os;
  PrintTo(resp, &os);
  EXPECT_EQ(os.str(), resp.DebugString());
}
}  // namespace
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
  TestPrintTo(request);
}

TEST(BinaryHttpRequest, DecodeGetNoBody) {
  const uint32_t words[] = {
      0x00034745, 0x54056874, 0x74707300, 0x0a2f6865, 0x6c6c6f2e, 0x74787440,
      0x6c0a7573, 0x65722d61, 0x67656e74, 0x34637572, 0x6c2f372e, 0x31362e33,
      0x206c6962, 0x6375726c, 0x2f372e31, 0x362e3320, 0x4f70656e, 0x53534c2f,
      0x302e392e, 0x376c207a, 0x6c69622f, 0x312e322e, 0x3304686f, 0x73740f77,
      0x77772e65, 0x78616d70, 0x6c652e63, 0x6f6d0f61, 0x63636570, 0x742d6c61,
      0x6e677561, 0x67650665, 0x6e2c206d, 0x69000000};
  std::string data;
  for (const auto& word : words) {
    data += WordToBytes(word);
  }

  // Remove all padding
  data.resize(data.size() - 3);

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
    TestPrintTo(field);
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
  TestPrintTo(request);
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
  TestPrintTo(request);
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
      TestPrintTo(field);
    }
    ASSERT_THAT(request.GetHeaderFields(), ContainerEq(expected_fields));
    ASSERT_EQ(request.body(), "");
    EXPECT_THAT(request.DebugString(),
                StrEq("BinaryHttpRequest{BinaryHttpMessage{Headers{}Body{}}}"));
    TestPrintTo(request);
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
  /*
    HTTP/1.1 404 Not Found
    Server: Apache
  */
  const uint32_t words[] = {0x0141940e, 0x06736572, 0x76657206, 0x41706163,
                            0x68650000};
  std::string data;
  for (const auto& word : words) {
    data += WordToBytes(word);
  }
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
  TestPrintTo(response);
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
  TestPrintTo(response);
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
void TestPadding(T& message) {
  const auto data_so = message.Serialize();
  ASSERT_TRUE(data_so.ok());
  auto data = *data_so;
  ASSERT_EQ(data.size(), message.EncodedSize());

  message.set_num_padding_bytes(10);
  const auto padded_data_so = message.Serialize();
  ASSERT_TRUE(padded_data_so.ok());
  const auto padded_data = *padded_data_so;
  ASSERT_EQ(padded_data.size(), message.EncodedSize());

  // Check padding size output.
  ASSERT_EQ(data.size() + 10, padded_data.size());
  // Check for valid null byte padding output
  data.resize(data.size() + 10);
  ASSERT_EQ(data, padded_data);

  // Deserialize padded and not padded, and verify they are the same.
  const auto deserialized_padded_message_so = T::Create(data);
  ASSERT_TRUE(deserialized_padded_message_so.ok());
  const auto deserialized_padded_message = *deserialized_padded_message_so;
  ASSERT_EQ(deserialized_padded_message, message);
  ASSERT_EQ(deserialized_padded_message.num_padding_bytes(), size_t(10));

  // Invalid padding
  data[data.size() - 1] = 'a';
  const auto bad_so = T::Create(data);
  ASSERT_FALSE(bad_so.ok());

  // Check that padding does not impact equality.
  data.resize(data.size() - 10);
  const auto deserialized_message_so = T::Create(data);
  ASSERT_TRUE(deserialized_message_so.ok());
  const auto deserialized_message = *deserialized_message_so;
  ASSERT_EQ(deserialized_message.num_padding_bytes(), size_t(0));
  // Confirm that the message payloads are equal, but not fully equivalent due
  // to padding.
  ASSERT_THAT(deserialized_message, HasEqPayload(deserialized_padded_message));
  ASSERT_NE(deserialized_message, deserialized_padded_message);
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
  TestPadding(request);
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
  TestPadding(response);
}

}  // namespace quiche
