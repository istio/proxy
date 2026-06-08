// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "absl/functional/overload.h"
#include "quiche/quic/core/qpack/qpack_encoder_stream_sender.h"
#include "quiche/quic/core/qpack/qpack_instruction_encoder.h"
#include "quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"

namespace quic {
namespace test {

namespace {

class FuzzAction {
 public:
  struct SendInsertWithNameReference {
    bool is_static;
    uint64_t name_index;
    uint16_t value_length;
    std::string value;
  };
  struct SendInsertWithoutNameReference {
    std::string name;
    std::string value;
  };
  struct SendDuplicate {
    uint64_t index;
  };
  struct SendSetDynamicTableCapacity {
    uint64_t capacity;
  };
  struct Flush {};

  using Variant =
      std::variant<SendInsertWithNameReference, SendInsertWithoutNameReference,
                   SendDuplicate, SendSetDynamicTableCapacity, Flush>;
};

// This fuzzer exercises QpackEncoderStreamSender.
void DoesNotCrash(HuffmanEncoding huffman_encoding,
                  const std::vector<FuzzAction::Variant>& actions) {
  NoopQpackStreamSenderDelegate delegate;
  QpackEncoderStreamSender sender(huffman_encoding);
  sender.set_qpack_stream_sender_delegate(&delegate);

  for (const FuzzAction::Variant& action : actions) {
    std::visit(
        absl::Overload{
            [&](const FuzzAction::SendInsertWithNameReference& insert) {
              sender.SendInsertWithNameReference(
                  insert.is_static, insert.name_index, insert.value);
            },
            [&](const FuzzAction::SendInsertWithoutNameReference& insert) {
              sender.SendInsertWithoutNameReference(insert.name, insert.value);
            },
            [&](const FuzzAction::SendDuplicate& duplicate) {
              sender.SendDuplicate(duplicate.index);
            },
            [&](const FuzzAction::SendSetDynamicTableCapacity& capacity) {
              sender.SendSetDynamicTableCapacity(capacity.capacity);
            },
            [&](const FuzzAction::Flush&) { sender.Flush(); },
        },
        action);
  }

  sender.Flush();
}

// Limit string length to 2 KiB for efficiency.
auto ShortStringDomain() { return fuzztest::String().WithMaxSize(2048); }

FUZZ_TEST(QpackEncoderStreamSenderFuzzer, DoesNotCrash)
    .WithDomains(
        fuzztest::ElementOf({HuffmanEncoding::kEnabled,
                             HuffmanEncoding::kDisabled}),
        fuzztest::VectorOf(fuzztest::VariantOf(

            fuzztest::StructOf<FuzzAction::SendInsertWithNameReference>(
                fuzztest::Arbitrary<bool>(), fuzztest::Arbitrary<uint64_t>(),
                fuzztest::Arbitrary<uint16_t>(), ShortStringDomain()),
            fuzztest::StructOf<FuzzAction::SendInsertWithoutNameReference>(
                ShortStringDomain(), ShortStringDomain()),
            fuzztest::Arbitrary<FuzzAction::SendDuplicate>(),
            fuzztest::Arbitrary<FuzzAction::SendSetDynamicTableCapacity>(),
            fuzztest::Arbitrary<FuzzAction::Flush>())));

}  // namespace

}  // namespace test
}  // namespace quic
