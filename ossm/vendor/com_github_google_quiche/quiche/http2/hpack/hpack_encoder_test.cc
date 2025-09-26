// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/http2/hpack/hpack_encoder.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "quiche/http2/hpack/hpack_constants.h"
#include "quiche/http2/hpack/hpack_entry.h"
#include "quiche/http2/hpack/hpack_header_table.h"
#include "quiche/http2/hpack/hpack_output_stream.h"
#include "quiche/http2/hpack/hpack_static_table.h"
#include "quiche/http2/hpack/huffman/hpack_huffman_encoder.h"
#include "quiche/http2/test_tools/http2_random.h"
#include "quiche/common/http/http_header_block.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_simple_arena.h"

namespace spdy {

namespace test {

class HpackHeaderTablePeer {
 public:
  explicit HpackHeaderTablePeer(HpackHeaderTable* table) : table_(table) {}

  const HpackEntry* GetFirstStaticEntry() const {
    return &table_->static_entries_.front();
  }

  HpackHeaderTable::DynamicEntryTable* dynamic_entries() {
    return &table_->dynamic_entries_;
  }

 private:
  HpackHeaderTable* table_;
};

class HpackEncoderPeer {
 public:
  typedef HpackEncoder::Representation Representation;
  typedef HpackEncoder::Representations Representations;

  explicit HpackEncoderPeer(HpackEncoder* encoder) : encoder_(encoder) {}

  bool dynamic_table_enabled() const { return encoder_->enable_dynamic_table_; }
  bool huffman_enabled() const { return encoder_->enable_huffman_; }
  HpackHeaderTable* table() { return &encoder_->header_table_; }
  HpackHeaderTablePeer table_peer() { return HpackHeaderTablePeer(table()); }
  void EmitString(absl::string_view str) { encoder_->EmitString(str); }
  void TakeString(std::string* out) {
    *out = encoder_->output_stream_.TakeString();
  }
  static void CookieToCrumbs(absl::string_view cookie,
                             std::vector<absl::string_view>* out) {
    Representations tmp;
    HpackEncoder::CookieToCrumbs(std::make_pair("", cookie), &tmp);

    out->clear();
    for (size_t i = 0; i != tmp.size(); ++i) {
      out->push_back(tmp[i].second);
    }
  }
  static void DecomposeRepresentation(absl::string_view value,
                                      std::vector<absl::string_view>* out) {
    Representations tmp;
    HpackEncoder::DecomposeRepresentation(std::make_pair("foobar", value),
                                          &tmp);

    out->clear();
    for (size_t i = 0; i != tmp.size(); ++i) {
      out->push_back(tmp[i].second);
    }
  }

  // TODO(dahollings): Remove or clean up these methods when deprecating
  // non-incremental encoding path.
  static std::string EncodeHeaderBlock(
      HpackEncoder* encoder, const quiche::HttpHeaderBlock& header_set) {
    return encoder->EncodeHeaderBlock(header_set);
  }

  static bool EncodeIncremental(HpackEncoder* encoder,
                                const quiche::HttpHeaderBlock& header_set,
                                std::string* output) {
    std::unique_ptr<HpackEncoder::ProgressiveEncoder> encoderator =
        encoder->EncodeHeaderSet(header_set);
    http2::test::Http2Random random;
    std::string output_buffer = encoderator->Next(random.UniformInRange(0, 16));
    while (encoderator->HasNext()) {
      std::string second_buffer =
          encoderator->Next(random.UniformInRange(0, 16));
      output_buffer.append(second_buffer);
    }
    *output = std::move(output_buffer);
    return true;
  }

  static bool EncodeRepresentations(HpackEncoder* encoder,
                                    const Representations& representations,
                                    std::string* output) {
    std::unique_ptr<HpackEncoder::ProgressiveEncoder> encoderator =
        encoder->EncodeRepresentations(representations);
    http2::test::Http2Random random;
    std::string output_buffer = encoderator->Next(random.UniformInRange(0, 16));
    while (encoderator->HasNext()) {
      std::string second_buffer =
          encoderator->Next(random.UniformInRange(0, 16));
      output_buffer.append(second_buffer);
    }
    *output = std::move(output_buffer);
    return true;
  }

 private:
  HpackEncoder* encoder_;
};

}  // namespace test

namespace {

using testing::ElementsAre;
using testing::Pair;

const size_t kStaticEntryIndex = 1;

enum EncodeStrategy {
  kDefault,
  kIncremental,
  kRepresentations,
};

class HpackEncoderTest
    : public quiche::test::QuicheTestWithParam<EncodeStrategy> {
 protected:
  typedef test::HpackEncoderPeer::Representations Representations;

  HpackEncoderTest()
      : peer_(&encoder_),
        static_(peer_.table_peer().GetFirstStaticEntry()),
        dynamic_table_insertions_(0),
        headers_storage_(1024 /* block size */),
        strategy_(GetParam()) {}

  void SetUp() override {
    // Populate dynamic entries into the table fixture. For simplicity each
    // entry has name.size() + value.size() == 10.
    key_1_ = peer_.table()->TryAddEntry("key1", "value1");
    key_1_index_ = dynamic_table_insertions_++;
    key_2_ = peer_.table()->TryAddEntry("key2", "value2");
    key_2_index_ = dynamic_table_insertions_++;
    cookie_a_ = peer_.table()->TryAddEntry("cookie", "a=bb");
    cookie_a_index_ = dynamic_table_insertions_++;
    cookie_c_ = peer_.table()->TryAddEntry("cookie", "c=dd");
    cookie_c_index_ = dynamic_table_insertions_++;

    // No further insertions may occur without evictions.
    peer_.table()->SetMaxSize(peer_.table()->size());
    QUICHE_CHECK_EQ(kInitialDynamicTableSize, peer_.table()->size());
  }

  void SaveHeaders(absl::string_view name, absl::string_view value) {
    absl::string_view n(headers_storage_.Memdup(name.data(), name.size()),
                        name.size());
    absl::string_view v(headers_storage_.Memdup(value.data(), value.size()),
                        value.size());
    headers_observed_.push_back(std::make_pair(n, v));
  }

  void ExpectIndex(size_t index) {
    expected_.AppendPrefix(kIndexedOpcode);
    expected_.AppendUint32(index);
  }
  void ExpectIndexedLiteral(size_t key_index, absl::string_view value) {
    expected_.AppendPrefix(kLiteralIncrementalIndexOpcode);
    expected_.AppendUint32(key_index);
    ExpectString(&expected_, value);
  }
  void ExpectIndexedLiteral(absl::string_view name, absl::string_view value) {
    expected_.AppendPrefix(kLiteralIncrementalIndexOpcode);
    expected_.AppendUint32(0);
    ExpectString(&expected_, name);
    ExpectString(&expected_, value);
  }
  void ExpectNonIndexedLiteral(absl::string_view name,
                               absl::string_view value) {
    expected_.AppendPrefix(kLiteralNoIndexOpcode);
    expected_.AppendUint32(0);
    ExpectString(&expected_, name);
    ExpectString(&expected_, value);
  }
  void ExpectNonIndexedLiteralWithNameIndex(size_t key_index,
                                            absl::string_view value) {
    expected_.AppendPrefix(kLiteralNoIndexOpcode);
    expected_.AppendUint32(key_index);
    ExpectString(&expected_, value);
  }
  void ExpectString(HpackOutputStream* stream, absl::string_view str) {
    size_t encoded_size =
        peer_.huffman_enabled() ? http2::HuffmanSize(str) : str.size();
    if (encoded_size < str.size()) {
      expected_.AppendPrefix(kStringLiteralHuffmanEncoded);
      expected_.AppendUint32(encoded_size);
      http2::HuffmanEncode(str, encoded_size, stream->MutableString());
    } else {
      expected_.AppendPrefix(kStringLiteralIdentityEncoded);
      expected_.AppendUint32(str.size());
      expected_.AppendBytes(str);
    }
  }
  void ExpectHeaderTableSizeUpdate(uint32_t size) {
    expected_.AppendPrefix(kHeaderTableSizeUpdateOpcode);
    expected_.AppendUint32(size);
  }
  Representations MakeRepresentations(
      const quiche::HttpHeaderBlock& header_set) {
    Representations r;
    for (const auto& header : header_set) {
      r.push_back(header);
    }
    return r;
  }
  void CompareWithExpectedEncoding(const quiche::HttpHeaderBlock& header_set) {
    std::string actual_out;
    std::string expected_out = expected_.TakeString();
    switch (strategy_) {
      case kDefault:
        actual_out =
            test::HpackEncoderPeer::EncodeHeaderBlock(&encoder_, header_set);
        break;
      case kIncremental:
        EXPECT_TRUE(test::HpackEncoderPeer::EncodeIncremental(
            &encoder_, header_set, &actual_out));
        break;
      case kRepresentations:
        EXPECT_TRUE(test::HpackEncoderPeer::EncodeRepresentations(
            &encoder_, MakeRepresentations(header_set), &actual_out));
        break;
    }
    EXPECT_EQ(expected_out, actual_out);
  }
  void CompareWithExpectedEncoding(const Representations& representations) {
    std::string actual_out;
    std::string expected_out = expected_.TakeString();
    EXPECT_TRUE(test::HpackEncoderPeer::EncodeRepresentations(
        &encoder_, representations, &actual_out));
    EXPECT_EQ(expected_out, actual_out);
  }
  // Converts the index of a dynamic table entry to the HPACK index.
  // In these test, dynamic table entries are indexed sequentially, starting
  // with 0.  The HPACK indexing scheme is defined at
  // https://httpwg.org/specs/rfc7541.html#index.address.space.
  size_t DynamicIndexToWireIndex(size_t index) {
    return dynamic_table_insertions_ - index + kStaticTableSize;
  }

  HpackEncoder encoder_;
  test::HpackEncoderPeer peer_;

  // Calculated based on the names and values inserted in SetUp(), above.
  const size_t kInitialDynamicTableSize = 4 * (10 + 32);

  const HpackEntry* static_;
  const HpackEntry* key_1_;
  const HpackEntry* key_2_;
  const HpackEntry* cookie_a_;
  const HpackEntry* cookie_c_;
  size_t key_1_index_;
  size_t key_2_index_;
  size_t cookie_a_index_;
  size_t cookie_c_index_;
  size_t dynamic_table_insertions_;

  quiche::QuicheSimpleArena headers_storage_;
  std::vector<std::pair<absl::string_view, absl::string_view>>
      headers_observed_;

  HpackOutputStream expected_;
  const EncodeStrategy strategy_;
};

using HpackEncoderTestWithDefaultStrategy = HpackEncoderTest;

INSTANTIATE_TEST_SUITE_P(HpackEncoderTests, HpackEncoderTestWithDefaultStrategy,
                         ::testing::Values(kDefault));

TEST_P(HpackEncoderTestWithDefaultStrategy, EncodeRepresentations) {
  EXPECT_EQ(kInitialDynamicTableSize, encoder_.GetDynamicTableSize());
  encoder_.SetHeaderListener(
      [this](absl::string_view name, absl::string_view value) {
        this->SaveHeaders(name, value);
      });
  const std::vector<std::pair<absl::string_view, absl::string_view>>
      header_list = {{"cookie", "val1; val2;val3"},
                     {":path", "/home"},
                     {"accept", "text/html, text/plain,application/xml"},
                     {"cookie", "val4"},
                     {"withnul", absl::string_view("one\0two", 7)}};
  ExpectNonIndexedLiteralWithNameIndex(peer_.table()->GetByName(":path"),
                                       "/home");
  ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "val1");
  ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "val2");
  ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "val3");
  ExpectIndexedLiteral(peer_.table()->GetByName("accept"),
                       "text/html, text/plain,application/xml");
  ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "val4");
  ExpectIndexedLiteral("withnul", absl::string_view("one\0two", 7));

  CompareWithExpectedEncoding(header_list);
  EXPECT_THAT(
      headers_observed_,
      ElementsAre(Pair(":path", "/home"), Pair("cookie", "val1"),
                  Pair("cookie", "val2"), Pair("cookie", "val3"),
                  Pair("accept", "text/html, text/plain,application/xml"),
                  Pair("cookie", "val4"),
                  Pair("withnul", absl::string_view("one\0two", 7))));
  // Insertions and evictions have happened over the course of the test.
  EXPECT_GE(kInitialDynamicTableSize, encoder_.GetDynamicTableSize());
}

TEST_P(HpackEncoderTestWithDefaultStrategy, WithoutCookieCrumbling) {
  EXPECT_EQ(kInitialDynamicTableSize, encoder_.GetDynamicTableSize());
  encoder_.SetHeaderListener(
      [this](absl::string_view name, absl::string_view value) {
        this->SaveHeaders(name, value);
      });
  encoder_.DisableCookieCrumbling();

  const std::vector<std::pair<absl::string_view, absl::string_view>>
      header_list = {{"cookie", "val1; val2;val3"},
                     {":path", "/home"},
                     {"accept", "text/html, text/plain,application/xml"},
                     {"cookie", "val4"},
                     {"withnul", absl::string_view("one\0two", 7)}};
  ExpectNonIndexedLiteralWithNameIndex(peer_.table()->GetByName(":path"),
                                       "/home");
  ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "val1; val2;val3");
  ExpectIndexedLiteral(peer_.table()->GetByName("accept"),
                       "text/html, text/plain,application/xml");
  ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "val4");
  ExpectIndexedLiteral("withnul", absl::string_view("one\0two", 7));

  CompareWithExpectedEncoding(header_list);
  EXPECT_THAT(
      headers_observed_,
      ElementsAre(Pair(":path", "/home"), Pair("cookie", "val1; val2;val3"),
                  Pair("accept", "text/html, text/plain,application/xml"),
                  Pair("cookie", "val4"),
                  Pair("withnul", absl::string_view("one\0two", 7))));
  // Insertions and evictions have happened over the course of the test.
  EXPECT_GE(kInitialDynamicTableSize, encoder_.GetDynamicTableSize());
}

TEST_P(HpackEncoderTestWithDefaultStrategy, DynamicTableGrows) {
  EXPECT_EQ(kInitialDynamicTableSize, encoder_.GetDynamicTableSize());
  peer_.table()->SetMaxSize(4096);
  encoder_.SetHeaderListener(
      [this](absl::string_view name, absl::string_view value) {
        this->SaveHeaders(name, value);
      });
  const std::vector<std::pair<absl::string_view, absl::string_view>>
      header_list = {{"cookie", "val1; val2;val3"},
                     {":path", "/home"},
                     {"accept", "text/html, text/plain,application/xml"},
                     {"cookie", "val4"},
                     {"withnul", absl::string_view("one\0two", 7)}};
  std::string out;
  EXPECT_TRUE(test::HpackEncoderPeer::EncodeRepresentations(&encoder_,
                                                            header_list, &out));

  EXPECT_FALSE(out.empty());
  // Insertions have happened over the course of the test.
  EXPECT_GT(encoder_.GetDynamicTableSize(), kInitialDynamicTableSize);
}

TEST_P(HpackEncoderTestWithDefaultStrategy, DynamicTableStableWithUpperBound) {
  EXPECT_EQ(kInitialDynamicTableSize, encoder_.GetDynamicTableSize());
  peer_.table()->SetMaxSize(4096);

  // Caps the dynamic table size at no larger than the initial size.
  encoder_.SetHeaderTableSizeBound(kInitialDynamicTableSize);

  encoder_.SetHeaderListener(
      [this](absl::string_view name, absl::string_view value) {
        this->SaveHeaders(name, value);
      });
  const std::vector<std::pair<absl::string_view, absl::string_view>>
      header_list = {{"cookie", "val1; val2;val3"},
                     {":path", "/home"},
                     {"accept", "text/html, text/plain,application/xml"},
                     {"cookie", "val4"},
                     {"withnul", absl::string_view("one\0two", 7)}};
  std::string out;
  EXPECT_TRUE(test::HpackEncoderPeer::EncodeRepresentations(&encoder_,
                                                            header_list, &out));

  EXPECT_FALSE(out.empty());
  // Insertions have happened over the course of the test, but the table is not
  // any larger.
  EXPECT_LE(encoder_.GetDynamicTableSize(), kInitialDynamicTableSize);
}

INSTANTIATE_TEST_SUITE_P(HpackEncoderTests, HpackEncoderTest,
                         ::testing::Values(kDefault, kIncremental,
                                           kRepresentations));

TEST_P(HpackEncoderTest, SingleDynamicIndex) {
  encoder_.SetHeaderListener(
      [this](absl::string_view name, absl::string_view value) {
        this->SaveHeaders(name, value);
      });

  ExpectIndex(DynamicIndexToWireIndex(key_2_index_));

  quiche::HttpHeaderBlock headers;
  headers[key_2_->name()] = key_2_->value();
  CompareWithExpectedEncoding(headers);
  EXPECT_THAT(headers_observed_,
              ElementsAre(Pair(key_2_->name(), key_2_->value())));
}

TEST_P(HpackEncoderTest, SingleStaticIndex) {
  ExpectIndex(kStaticEntryIndex);

  quiche::HttpHeaderBlock headers;
  headers[static_->name()] = static_->value();
  CompareWithExpectedEncoding(headers);
}

TEST_P(HpackEncoderTest, SingleStaticIndexTooLarge) {
  peer_.table()->SetMaxSize(1);  // Also evicts all fixtures.
  ExpectIndex(kStaticEntryIndex);

  quiche::HttpHeaderBlock headers;
  headers[static_->name()] = static_->value();
  CompareWithExpectedEncoding(headers);

  EXPECT_EQ(0u, peer_.table_peer().dynamic_entries()->size());
}

TEST_P(HpackEncoderTest, SingleLiteralWithIndexName) {
  ExpectIndexedLiteral(DynamicIndexToWireIndex(key_2_index_), "value3");

  quiche::HttpHeaderBlock headers;
  headers[key_2_->name()] = "value3";
  CompareWithExpectedEncoding(headers);

  // A new entry was inserted and added to the reference set.
  HpackEntry* new_entry = peer_.table_peer().dynamic_entries()->front().get();
  EXPECT_EQ(new_entry->name(), key_2_->name());
  EXPECT_EQ(new_entry->value(), "value3");
}

TEST_P(HpackEncoderTest, SingleLiteralWithLiteralName) {
  ExpectIndexedLiteral("key3", "value3");

  quiche::HttpHeaderBlock headers;
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);

  HpackEntry* new_entry = peer_.table_peer().dynamic_entries()->front().get();
  EXPECT_EQ(new_entry->name(), "key3");
  EXPECT_EQ(new_entry->value(), "value3");
}

TEST_P(HpackEncoderTest, SingleLiteralTooLarge) {
  peer_.table()->SetMaxSize(1);  // Also evicts all fixtures.

  ExpectIndexedLiteral("key3", "value3");

  // A header overflowing the header table is still emitted.
  // The header table is empty.
  quiche::HttpHeaderBlock headers;
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);

  EXPECT_EQ(0u, peer_.table_peer().dynamic_entries()->size());
}

TEST_P(HpackEncoderTest, EmitThanEvict) {
  // |key_1_| is toggled and placed into the reference set,
  // and then immediately evicted by "key3".
  ExpectIndex(DynamicIndexToWireIndex(key_1_index_));
  ExpectIndexedLiteral("key3", "value3");

  quiche::HttpHeaderBlock headers;
  headers[key_1_->name()] = key_1_->value();
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);
}

TEST_P(HpackEncoderTest, CookieHeaderIsCrumbled) {
  ExpectIndex(DynamicIndexToWireIndex(cookie_a_index_));
  ExpectIndex(DynamicIndexToWireIndex(cookie_c_index_));
  ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "e=ff");

  quiche::HttpHeaderBlock headers;
  headers["cookie"] = "a=bb; c=dd; e=ff";
  CompareWithExpectedEncoding(headers);
}

TEST_P(HpackEncoderTest, CookieHeaderIsNotCrumbled) {
  encoder_.DisableCookieCrumbling();
  ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "a=bb; c=dd; e=ff");

  quiche::HttpHeaderBlock headers;
  headers["cookie"] = "a=bb; c=dd; e=ff";
  CompareWithExpectedEncoding(headers);
}

TEST_P(HpackEncoderTest, MultiValuedHeadersNotCrumbled) {
  ExpectIndexedLiteral("foo", "bar, baz");
  quiche::HttpHeaderBlock headers;
  headers["foo"] = "bar, baz";
  CompareWithExpectedEncoding(headers);
}

TEST_P(HpackEncoderTest, StringsDynamicallySelectHuffmanCoding) {
  // Compactable string. Uses Huffman coding.
  peer_.EmitString("feedbeef");
  expected_.AppendPrefix(kStringLiteralHuffmanEncoded);
  expected_.AppendUint32(6);
  expected_.AppendBytes("\x94\xA5\x92\x32\x96_");

  // Non-compactable. Uses identity coding.
  peer_.EmitString("@@@@@@");
  expected_.AppendPrefix(kStringLiteralIdentityEncoded);
  expected_.AppendUint32(6);
  expected_.AppendBytes("@@@@@@");

  std::string actual_out;
  std::string expected_out = expected_.TakeString();
  peer_.TakeString(&actual_out);
  EXPECT_EQ(expected_out, actual_out);
}

TEST_P(HpackEncoderTest, StringEncodingWhenHuffmanDisabled) {
  encoder_.DisableHuffman();
  // Compactable string, but will not use Huffman.
  peer_.EmitString("feedbeef");
  expected_.AppendPrefix(kStringLiteralIdentityEncoded);
  expected_.AppendUint32(8);
  expected_.AppendBytes("feedbeef");

  std::string actual_out;
  std::string expected_out = expected_.TakeString();
  peer_.TakeString(&actual_out);
  EXPECT_EQ(expected_out, actual_out);
}

TEST_P(HpackEncoderTest, EncodingWithoutCompression) {
  encoder_.SetHeaderListener(
      [this](absl::string_view name, absl::string_view value) {
        this->SaveHeaders(name, value);
      });
  encoder_.DisableCompression();

  ExpectNonIndexedLiteral(":path", "/index.html");
  ExpectNonIndexedLiteral("cookie", "foo=bar");
  ExpectNonIndexedLiteral("cookie", "baz=bing");
  if (strategy_ == kRepresentations) {
    ExpectNonIndexedLiteral("hello", std::string("goodbye\0aloha", 13));
  } else {
    ExpectNonIndexedLiteral("hello", "goodbye");
    ExpectNonIndexedLiteral("hello", "aloha");
  }
  ExpectNonIndexedLiteral("multivalue", "value1, value2");

  quiche::HttpHeaderBlock headers;
  headers[":path"] = "/index.html";
  headers["cookie"] = "foo=bar; baz=bing";
  headers["hello"] = "goodbye";
  headers.AppendValueOrAddHeader("hello", "aloha");
  headers["multivalue"] = "value1, value2";

  CompareWithExpectedEncoding(headers);

  if (strategy_ == kRepresentations) {
    EXPECT_THAT(
        headers_observed_,
        ElementsAre(Pair(":path", "/index.html"), Pair("cookie", "foo=bar"),
                    Pair("cookie", "baz=bing"),
                    Pair("hello", absl::string_view("goodbye\0aloha", 13)),
                    Pair("multivalue", "value1, value2")));
  } else {
    EXPECT_THAT(
        headers_observed_,
        ElementsAre(Pair(":path", "/index.html"), Pair("cookie", "foo=bar"),
                    Pair("cookie", "baz=bing"), Pair("hello", "goodbye"),
                    Pair("hello", "aloha"),
                    Pair("multivalue", "value1, value2")));
  }
  EXPECT_EQ(kInitialDynamicTableSize, encoder_.GetDynamicTableSize());
}

TEST_P(HpackEncoderTest, EncodingWithoutHuffman) {
  encoder_.SetHeaderListener(
      [this](absl::string_view name, absl::string_view value) {
        this->SaveHeaders(name, value);
      });
  encoder_.DisableHuffman();
  EXPECT_FALSE(peer_.huffman_enabled());

  // Static table entry: ":path", "/index.html"
  ExpectIndex(5);
  // Static table name entry: "cookie"
  ExpectIndexedLiteral(32, "foo=bar");
  ++dynamic_table_insertions_;
  ExpectIndexedLiteral(32, "baz=bing");
  ++dynamic_table_insertions_;
  if (strategy_ == kRepresentations) {
    ExpectIndexedLiteral("hello", std::string("goodbye\0aloha", 13));
  } else {
    ExpectIndexedLiteral("hello", "goodbye");
    const size_t hello_index = dynamic_table_insertions_++;
    // Dynamic table name entry: "hello"
    ExpectIndexedLiteral(DynamicIndexToWireIndex(hello_index), "aloha");
  }
  ExpectIndexedLiteral("multivalue", "value1, value2");

  quiche::HttpHeaderBlock headers;
  headers[":path"] = "/index.html";
  headers["cookie"] = "foo=bar; baz=bing";
  headers["hello"] = "goodbye";
  headers.AppendValueOrAddHeader("hello", "aloha");
  headers["multivalue"] = "value1, value2";

  CompareWithExpectedEncoding(headers);

  if (strategy_ == kRepresentations) {
    EXPECT_THAT(
        headers_observed_,
        ElementsAre(Pair(":path", "/index.html"), Pair("cookie", "foo=bar"),
                    Pair("cookie", "baz=bing"),
                    Pair("hello", absl::string_view("goodbye\0aloha", 13)),
                    Pair("multivalue", "value1, value2")));
  } else {
    EXPECT_THAT(
        headers_observed_,
        ElementsAre(Pair(":path", "/index.html"), Pair("cookie", "foo=bar"),
                    Pair("cookie", "baz=bing"), Pair("hello", "goodbye"),
                    Pair("hello", "aloha"),
                    Pair("multivalue", "value1, value2")));
  }
}

TEST_P(HpackEncoderTest, MultipleEncodingPasses) {
  encoder_.SetHeaderListener(
      [this](absl::string_view name, absl::string_view value) {
        this->SaveHeaders(name, value);
      });

  // Pass 1.
  {
    quiche::HttpHeaderBlock headers;
    headers["key1"] = "value1";
    headers["cookie"] = "a=bb";

    ExpectIndex(DynamicIndexToWireIndex(key_1_index_));
    ExpectIndex(DynamicIndexToWireIndex(cookie_a_index_));
    CompareWithExpectedEncoding(headers);
  }
  // Header table is:
  // 65: key1: value1
  // 64: key2: value2
  // 63: cookie: a=bb
  // 62: cookie: c=dd
  // Pass 2.
  {
    quiche::HttpHeaderBlock headers;
    headers["key2"] = "value2";
    headers["cookie"] = "c=dd; e=ff";

    // "key2: value2"
    ExpectIndex(DynamicIndexToWireIndex(key_2_index_));
    // "cookie: c=dd"
    ExpectIndex(DynamicIndexToWireIndex(cookie_c_index_));
    // This cookie evicts |key1| from the dynamic table.
    ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "e=ff");
    dynamic_table_insertions_++;

    CompareWithExpectedEncoding(headers);
  }
  // Header table is:
  // 65: key2: value2
  // 64: cookie: a=bb
  // 63: cookie: c=dd
  // 62: cookie: e=ff
  // Pass 3.
  {
    quiche::HttpHeaderBlock headers;
    headers["key2"] = "value2";
    headers["cookie"] = "a=bb; b=cc; c=dd";

    // "key2: value2"
    EXPECT_EQ(65u, DynamicIndexToWireIndex(key_2_index_));
    ExpectIndex(DynamicIndexToWireIndex(key_2_index_));
    // "cookie: a=bb"
    EXPECT_EQ(64u, DynamicIndexToWireIndex(cookie_a_index_));
    ExpectIndex(DynamicIndexToWireIndex(cookie_a_index_));
    // This cookie evicts |key2| from the dynamic table.
    ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "b=cc");
    dynamic_table_insertions_++;
    // "cookie: c=dd"
    ExpectIndex(DynamicIndexToWireIndex(cookie_c_index_));

    CompareWithExpectedEncoding(headers);
  }

  // clang-format off
  EXPECT_THAT(headers_observed_,
              ElementsAre(Pair("key1", "value1"),
                          Pair("cookie", "a=bb"),
                          Pair("key2", "value2"),
                          Pair("cookie", "c=dd"),
                          Pair("cookie", "e=ff"),
                          Pair("key2", "value2"),
                          Pair("cookie", "a=bb"),
                          Pair("cookie", "b=cc"),
                          Pair("cookie", "c=dd")));
  // clang-format on
}

TEST_P(HpackEncoderTest, PseudoHeadersFirst) {
  quiche::HttpHeaderBlock headers;
  // A pseudo-header that should not be indexed.
  headers[":path"] = "/spam/eggs.html";
  // A pseudo-header to be indexed.
  headers[":authority"] = "www.example.com";
  // A regular header which precedes ":" alphabetically, should still be encoded
  // after pseudo-headers.
  headers["-foo"] = "bar";
  headers["foo"] = "bar";
  headers["cookie"] = "c=dd";

  // Headers are indexed in the order in which they were added.
  // This entry pushes "cookie: a=bb" back to 63.
  ExpectNonIndexedLiteralWithNameIndex(peer_.table()->GetByName(":path"),
                                       "/spam/eggs.html");
  ExpectIndexedLiteral(peer_.table()->GetByName(":authority"),
                       "www.example.com");
  ExpectIndexedLiteral("-foo", "bar");
  ExpectIndexedLiteral("foo", "bar");
  ExpectIndexedLiteral(peer_.table()->GetByName("cookie"), "c=dd");
  CompareWithExpectedEncoding(headers);
}

TEST_P(HpackEncoderTest, CookieToCrumbs) {
  test::HpackEncoderPeer peer(nullptr);
  std::vector<absl::string_view> out;

  // Leading and trailing whitespace is consumed. A space after ';' is consumed.
  // All other spaces remain. ';' at beginning and end of string produce empty
  // crumbs.
  // See section 8.1.3.4 "Compressing the Cookie Header Field" in the HTTP/2
  // specification at http://tools.ietf.org/html/draft-ietf-httpbis-http2-11
  peer.CookieToCrumbs(" foo=1;bar=2 ; bar=3;  bing=4; ", &out);
  EXPECT_THAT(out, ElementsAre("foo=1", "bar=2 ", "bar=3", " bing=4", ""));

  peer.CookieToCrumbs(";;foo = bar ;; ;baz =bing", &out);
  EXPECT_THAT(out, ElementsAre("", "", "foo = bar ", "", "", "baz =bing"));

  peer.CookieToCrumbs("baz=bing; foo=bar; baz=bing", &out);
  EXPECT_THAT(out, ElementsAre("baz=bing", "foo=bar", "baz=bing"));

  peer.CookieToCrumbs("baz=bing", &out);
  EXPECT_THAT(out, ElementsAre("baz=bing"));

  peer.CookieToCrumbs("", &out);
  EXPECT_THAT(out, ElementsAre(""));

  peer.CookieToCrumbs("foo;bar; baz;baz;bing;", &out);
  EXPECT_THAT(out, ElementsAre("foo", "bar", "baz", "baz", "bing", ""));

  peer.CookieToCrumbs(" \t foo=1;bar=2 ; bar=3;\t  ", &out);
  EXPECT_THAT(out, ElementsAre("foo=1", "bar=2 ", "bar=3", ""));

  peer.CookieToCrumbs(" \t foo=1;bar=2 ; bar=3 \t  ", &out);
  EXPECT_THAT(out, ElementsAre("foo=1", "bar=2 ", "bar=3"));
}

TEST_P(HpackEncoderTest, DecomposeRepresentation) {
  test::HpackEncoderPeer peer(nullptr);
  std::vector<absl::string_view> out;

  peer.DecomposeRepresentation("", &out);
  EXPECT_THAT(out, ElementsAre(""));

  peer.DecomposeRepresentation("foobar", &out);
  EXPECT_THAT(out, ElementsAre("foobar"));

  peer.DecomposeRepresentation(absl::string_view("foo\0bar", 7), &out);
  EXPECT_THAT(out, ElementsAre("foo", "bar"));

  peer.DecomposeRepresentation(absl::string_view("\0foo\0bar", 8), &out);
  EXPECT_THAT(out, ElementsAre("", "foo", "bar"));

  peer.DecomposeRepresentation(absl::string_view("foo\0bar\0", 8), &out);
  EXPECT_THAT(out, ElementsAre("foo", "bar", ""));

  peer.DecomposeRepresentation(absl::string_view("\0foo\0bar\0", 9), &out);
  EXPECT_THAT(out, ElementsAre("", "foo", "bar", ""));
}

// Test that encoded headers do not have \0-delimited multiple values, as this
// became disallowed in HTTP/2 draft-14.
TEST_P(HpackEncoderTest, CrumbleNullByteDelimitedValue) {
  if (strategy_ == kRepresentations) {
    // When HpackEncoder is asked to encode a list of Representations, the
    // caller must crumble null-delimited values.
    return;
  }
  quiche::HttpHeaderBlock headers;
  // A header field to be crumbled: "spam: foo\0bar".
  headers["spam"] = std::string("foo\0bar", 7);

  ExpectIndexedLiteral("spam", "foo");
  expected_.AppendPrefix(kLiteralIncrementalIndexOpcode);
  expected_.AppendUint32(62);
  expected_.AppendPrefix(kStringLiteralIdentityEncoded);
  expected_.AppendUint32(3);
  expected_.AppendBytes("bar");
  CompareWithExpectedEncoding(headers);
}

TEST_P(HpackEncoderTest, HeaderTableSizeUpdate) {
  encoder_.ApplyHeaderTableSizeSetting(1024);
  ExpectHeaderTableSizeUpdate(1024);
  ExpectIndexedLiteral("key3", "value3");

  quiche::HttpHeaderBlock headers;
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);

  HpackEntry* new_entry = peer_.table_peer().dynamic_entries()->front().get();
  EXPECT_EQ(new_entry->name(), "key3");
  EXPECT_EQ(new_entry->value(), "value3");
}

TEST_P(HpackEncoderTest, HeaderTableSizeUpdateLessThanUpperBound) {
  encoder_.SetHeaderTableSizeBound(16 * 1024);
  encoder_.ApplyHeaderTableSizeSetting(1024);
  ExpectHeaderTableSizeUpdate(1024);
  ExpectIndexedLiteral("key3", "value3");

  quiche::HttpHeaderBlock headers;
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);

  HpackEntry* new_entry = peer_.table_peer().dynamic_entries()->front().get();
  EXPECT_EQ(new_entry->name(), "key3");
  EXPECT_EQ(new_entry->value(), "value3");
}

TEST_P(HpackEncoderTest, HeaderTableSizeUpdateGreaterThanUpperBound) {
  encoder_.SetHeaderTableSizeBound(512);
  encoder_.ApplyHeaderTableSizeSetting(1024);
  // Since the peer's advertised SETTINGS_HEADER_TABLE_SIZE is larger than our
  // upper bound, the encoder will limit its dynamic table size to the specified
  // upper bound.
  ExpectHeaderTableSizeUpdate(512);
  ExpectIndexedLiteral("key3", "value3");

  quiche::HttpHeaderBlock headers;
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);

  HpackEntry* new_entry = peer_.table_peer().dynamic_entries()->front().get();
  EXPECT_EQ(new_entry->name(), "key3");
  EXPECT_EQ(new_entry->value(), "value3");
}

TEST_P(HpackEncoderTest, HeaderTableSizeUpdateUpperBoundSmallerThenLarger) {
  encoder_.ApplyHeaderTableSizeSetting(1024);
  encoder_.SetHeaderTableSizeBound(512);
  // Since the table size upper bound is smaller than the value in SETTINGS, the
  // upper bound value takes precedence.
  ExpectHeaderTableSizeUpdate(512);
  ExpectIndexedLiteral("key3", "value3");

  quiche::HttpHeaderBlock headers;
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);

  HpackEntry* new_entry = peer_.table_peer().dynamic_entries()->front().get();
  EXPECT_EQ(new_entry->name(), "key3");
  EXPECT_EQ(new_entry->value(), "value3");

  encoder_.SetHeaderTableSizeBound(2 * 1024);
  // Now that the table size upper bound has been relaxed, the value from
  // SETTINGS is used.
  ExpectHeaderTableSizeUpdate(1024);
  ExpectIndex(peer_.table()->GetByNameAndValue("key3", "value3"));
  ExpectIndexedLiteral("key4", "value4");

  headers["key4"] = "value4";
  CompareWithExpectedEncoding(headers);

  new_entry = peer_.table_peer().dynamic_entries()->front().get();
  EXPECT_EQ(new_entry->name(), "key4");
  EXPECT_EQ(new_entry->value(), "value4");
}

TEST_P(HpackEncoderTest, HeaderTableSizeUpdateUpperBoundIsZero) {
  encoder_.ApplyHeaderTableSizeSetting(1024);
  encoder_.SetHeaderTableSizeBound(0);
  // A table size bound of 0 disables dynamic table compression.
  ExpectHeaderTableSizeUpdate(0);
  ExpectNonIndexedLiteral("key3", "value3");

  quiche::HttpHeaderBlock headers;
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);

  EXPECT_TRUE(peer_.table_peer().dynamic_entries()->empty());
}

TEST_P(HpackEncoderTest, HeaderTableSizeUpdateWithMin) {
  const size_t starting_size = peer_.table()->settings_size_bound();
  encoder_.ApplyHeaderTableSizeSetting(starting_size - 2);
  encoder_.ApplyHeaderTableSizeSetting(starting_size - 1);
  // We must encode the low watermark, so the peer knows to evict entries
  // if necessary.
  ExpectHeaderTableSizeUpdate(starting_size - 2);
  ExpectHeaderTableSizeUpdate(starting_size - 1);
  ExpectIndexedLiteral("key3", "value3");

  quiche::HttpHeaderBlock headers;
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);

  HpackEntry* new_entry = peer_.table_peer().dynamic_entries()->front().get();
  EXPECT_EQ(new_entry->name(), "key3");
  EXPECT_EQ(new_entry->value(), "value3");
}

TEST_P(HpackEncoderTest, HeaderTableSizeUpdateWithExistingSize) {
  encoder_.ApplyHeaderTableSizeSetting(peer_.table()->settings_size_bound());
  // No encoded size update.
  ExpectIndexedLiteral("key3", "value3");

  quiche::HttpHeaderBlock headers;
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);

  HpackEntry* new_entry = peer_.table_peer().dynamic_entries()->front().get();
  EXPECT_EQ(new_entry->name(), "key3");
  EXPECT_EQ(new_entry->value(), "value3");
}

TEST_P(HpackEncoderTest, HeaderTableSizeUpdatesWithGreaterSize) {
  const size_t starting_size = peer_.table()->settings_size_bound();
  encoder_.ApplyHeaderTableSizeSetting(starting_size + 1);
  encoder_.ApplyHeaderTableSizeSetting(starting_size + 2);
  // Only a single size update to the final size.
  ExpectHeaderTableSizeUpdate(starting_size + 2);
  ExpectIndexedLiteral("key3", "value3");

  quiche::HttpHeaderBlock headers;
  headers["key3"] = "value3";
  CompareWithExpectedEncoding(headers);

  HpackEntry* new_entry = peer_.table_peer().dynamic_entries()->front().get();
  EXPECT_EQ(new_entry->name(), "key3");
  EXPECT_EQ(new_entry->value(), "value3");
}

}  // namespace

}  // namespace spdy
