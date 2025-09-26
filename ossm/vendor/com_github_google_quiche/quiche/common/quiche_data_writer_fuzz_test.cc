// Copyright (c) 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <variant>
#include <vector>

#include "absl/functional/overload.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_data_reader.h"
#include "quiche/common/quiche_data_writer.h"
#include "quiche/common/quiche_endian.h"

namespace quiche {
namespace {

using fuzztest::Arbitrary;
using fuzztest::ElementOf;
using fuzztest::InRange;
using fuzztest::Map;
using fuzztest::StructOf;
using fuzztest::VariantOf;
using fuzztest::VectorOf;

class MethodFuzzUtil {
 public:
  struct CallWriteUInt8 {
    uint8_t value;
  };
  struct CallWriteUInt16 {
    uint16_t value;
  };
  struct CallWriteUInt32 {
    uint32_t value;
  };
  struct CallWriteUInt64 {
    uint64_t value;
  };
  struct CallWriteBytesToUInt64 {
    size_t num_bytes;
    uint64_t value;
  };
  struct CallWriteStringPiece {
    std::string value;
  };
  struct CallWriteStringPiece16 {
    std::string value;
  };
  struct CallWriteBytes {
    std::string data;
  };
  struct CallWriteRepeatedByte {
    uint8_t byte;
    size_t count;
  };
  struct CallWritePadding {};
  struct CallWritePaddingBytes {
    size_t count;
  };
  struct CallWriteTag {
    uint32_t tag;
  };
  struct CallWriteVarInt62 {
    uint64_t value;
  };
  struct CallWriteVarInt62WithForcedLength {
    uint64_t value;
    QuicheVariableLengthIntegerLength write_length;
  };
  struct CallWriteStringPieceVarInt62 {
    std::string value;
  };
  struct CallSeek {
    size_t length;
  };

  using CallVariant =
      std::variant<CallWriteUInt8, CallWriteUInt16, CallWriteUInt32,
                   CallWriteUInt64, CallWriteBytesToUInt64,
                   CallWriteStringPiece, CallWriteStringPiece16, CallWriteBytes,
                   CallWriteRepeatedByte, CallWritePadding,
                   CallWritePaddingBytes, CallWriteTag, CallWriteVarInt62,
                   CallWriteVarInt62WithForcedLength,
                   CallWriteStringPieceVarInt62, CallSeek>;

  static fuzztest::Domain<CallVariant> CallVariantDomain() {
    return VariantOf(
        Arbitrary<MethodFuzzUtil::CallWriteUInt8>(),
        Arbitrary<MethodFuzzUtil::CallWriteUInt16>(),
        Arbitrary<MethodFuzzUtil::CallWriteUInt32>(),
        Arbitrary<MethodFuzzUtil::CallWriteUInt64>(),
        StructOf<MethodFuzzUtil::CallWriteBytesToUInt64>(
            /*num_bytes=*/InRange<size_t>(1u, 8u),
            /*value=*/Arbitrary<uint64_t>()),
        Arbitrary<MethodFuzzUtil::CallWriteStringPiece>(),
        Arbitrary<MethodFuzzUtil::CallWriteStringPiece16>(),
        Arbitrary<MethodFuzzUtil::CallWriteBytes>(),
        StructOf<MethodFuzzUtil::CallWriteRepeatedByte>(
            /*byte=*/Arbitrary<uint8_t>(),
            /*count=*/InRange<size_t>(0u, 1u << 16u)),
        Arbitrary<MethodFuzzUtil::CallWritePadding>(),
        StructOf<MethodFuzzUtil::CallWritePaddingBytes>(
            InRange<size_t>(0u, 1u << 16u)),
        Arbitrary<MethodFuzzUtil::CallWriteTag>(),
        Arbitrary<MethodFuzzUtil::CallWriteVarInt62>(),
        Map(
            [](MethodFuzzUtil::CallWriteVarInt62WithForcedLength call) {
              // Increase `call.write_length` if `call.value` wouldn't fit.
              // This use of `std::max` is potentially fragile because it
              // depends on the comparability of the enum type
              // `QuicheVariableLengthIntegerLength`.
              call.write_length =
                  std::max(call.write_length,
                           QuicheDataWriter::GetVarInt62Len(call.value));
              return call;
            },
            StructOf<MethodFuzzUtil::CallWriteVarInt62WithForcedLength>(
                InRange<uint64_t>(0u, kVarInt62MaxValue),
                ElementOf(kAllQuicheVariableLengthIntegerLengths))),
        Arbitrary<MethodFuzzUtil::CallWriteStringPieceVarInt62>(),
        Arbitrary<MethodFuzzUtil::CallSeek>());
  }
};

// Interprets each element of `call_sequence` by calling the appropriate method
// of `QuicheDataWriter`. For each writer call, it also takes a corresponding
// action on a `QuicheDataReader` and makes a best effort to ensure that the
// writer and reader agree.
void WriterAndReaderStayInSync(
    size_t buffer_size, quiche::Endianness endianness,
    const std::vector<MethodFuzzUtil::CallVariant>& call_sequence) {
  std::vector<char> buffer(buffer_size);
  QuicheDataWriter writer(buffer.size(), buffer.data(), endianness);
  QuicheDataReader reader(buffer.data(), buffer.size(), endianness);

  for (const MethodFuzzUtil::CallVariant& call : call_sequence) {
    bool write_succeeded = true;
    std::visit(
        absl::Overload{
            [&](const MethodFuzzUtil::CallWriteUInt8& call) {
              if (!writer.WriteUInt8(call.value)) {
                write_succeeded = false;
                return;
              }
              uint8_t value;
              ASSERT_TRUE(reader.ReadUInt8(&value));
              ASSERT_EQ(call.value, value);
            },
            [&](const MethodFuzzUtil::CallWriteUInt16& call) {
              if (!writer.WriteUInt16(call.value)) {
                write_succeeded = false;
                return;
              }
              uint16_t value;
              ASSERT_TRUE(reader.ReadUInt16(&value));
              ASSERT_EQ(call.value, value);
            },
            [&](const MethodFuzzUtil::CallWriteUInt32& call) {
              if (!writer.WriteUInt32(call.value)) {
                write_succeeded = false;
                return;
              }
              uint32_t value;
              ASSERT_TRUE(reader.ReadUInt32(&value));
              ASSERT_EQ(call.value, value);
            },
            [&](const MethodFuzzUtil::CallWriteUInt64& call) {
              if (!writer.WriteUInt64(call.value)) {
                write_succeeded = false;
                return;
              }
              uint64_t value;
              ASSERT_TRUE(reader.ReadUInt64(&value));
              ASSERT_EQ(call.value, value);
            },
            [&](const MethodFuzzUtil::CallWriteBytesToUInt64& call) {
              if (!writer.WriteBytesToUInt64(call.num_bytes, call.value)) {
                write_succeeded = false;
                return;
              }
              // Ideally, we would test whether `parsed_value` has the expected
              // value, but it's difficult to compute the endianness-specific
              // least-significant bytes of `call.value` without reimplementing
              // a large part of `QuicheDataWriter::WriteBytesToUInt64()`.
              uint64_t parsed_value;
              ASSERT_TRUE(
                  reader.ReadBytesToUInt64(call.num_bytes, &parsed_value));
            },
            [&](const MethodFuzzUtil::CallWriteStringPiece& call) {
              if (!writer.WriteStringPiece(call.value)) {
                write_succeeded = false;
                return;
              }
              absl::string_view value;
              ASSERT_TRUE(reader.ReadStringPiece(&value, call.value.length()));
              ASSERT_EQ(call.value, value);
            },
            [&](const MethodFuzzUtil::CallWriteStringPiece16& call) {
              if (!writer.WriteStringPiece16(call.value)) {
                write_succeeded = false;
                return;
              }
              absl::string_view value;
              ASSERT_TRUE(reader.ReadStringPiece16(&value));
              ASSERT_EQ(call.value, value);
            },
            [&](const MethodFuzzUtil::CallWriteBytes& call) {
              if (!writer.WriteBytes(call.data.data(), call.data.length())) {
                write_succeeded = false;
                return;
              }
              std::string temp(call.data.length(), '\0');
              ASSERT_TRUE(reader.ReadBytes(temp.data(), temp.length()));
              ASSERT_EQ(call.data, temp);
            },
            [&](const MethodFuzzUtil::CallWriteRepeatedByte& call) {
              if (!writer.WriteRepeatedByte(call.byte, call.count)) {
                write_succeeded = false;
                return;
              }
              absl::string_view value;
              ASSERT_TRUE(reader.ReadStringPiece(&value, call.count));
              ASSERT_THAT(value, testing::Each(static_cast<char>(call.byte)));
            },
            [&](const MethodFuzzUtil::CallWritePadding&) {
              size_t remaining = writer.remaining();
              writer.WritePadding();
              absl::string_view value = reader.ReadRemainingPayload();
              ASSERT_EQ(value.length(), remaining);
              ASSERT_THAT(value, testing::Each('\0'));
            },
            [&](const MethodFuzzUtil::CallWritePaddingBytes& call) {
              if (!writer.WritePaddingBytes(call.count)) {
                write_succeeded = false;
                return;
              }
              absl::string_view value;
              ASSERT_TRUE(reader.ReadStringPiece(&value, call.count));
              ASSERT_THAT(value, testing::Each('\0'));
            },
            [&](const MethodFuzzUtil::CallWriteTag& call) {
              if (!writer.WriteTag(call.tag)) {
                write_succeeded = false;
                return;
              }
              uint32_t value;
              ASSERT_TRUE(reader.ReadTag(&value));
              ASSERT_EQ(call.tag, value);
            },
            [&](const MethodFuzzUtil::CallWriteVarInt62& call) {
              if (endianness != NETWORK_BYTE_ORDER) {
                return;
              }
              if (!writer.WriteVarInt62(call.value)) {
                write_succeeded = false;
                return;
              }
              uint64_t value;
              ASSERT_TRUE(reader.ReadVarInt62(&value));
              ASSERT_EQ(call.value, value);
            },
            [&](const MethodFuzzUtil::CallWriteVarInt62WithForcedLength& call) {
              if (endianness != NETWORK_BYTE_ORDER) {
                return;
              }
              if (!writer.WriteVarInt62WithForcedLength(call.value,
                                                        call.write_length)) {
                write_succeeded = false;
                return;
              }
              uint64_t value;
              ASSERT_TRUE(reader.ReadVarInt62(&value));
              ASSERT_EQ(call.value, value);
            },
            [&](const MethodFuzzUtil::CallWriteStringPieceVarInt62& call) {
              if (endianness != NETWORK_BYTE_ORDER) {
                return;
              }
              if (!writer.WriteStringPieceVarInt62(call.value)) {
                write_succeeded = false;
                return;
              }
              absl::string_view value;
              ASSERT_TRUE(reader.ReadStringPieceVarInt62(&value));
              ASSERT_EQ(call.value, value);
            },
            [&](const MethodFuzzUtil::CallSeek& call) {
              if (!writer.Seek(call.length)) {
                write_succeeded = false;
                return;
              }
              ASSERT_TRUE(reader.Seek(call.length));
            },
        },
        call);

    // `QuicheDataWriter` operations do not guarantee atomicity. For instance,
    // when `WriteStringPiece16()` fails, it may have successfully written the
    // length prefix, but failed to write the payload. As a consequence, after a
    // write operation fails, we cannot assume that the reader and writer will
    // still be in sync.
    if (!write_succeeded) {
      break;
    }
    ASSERT_EQ(writer.remaining(), reader.BytesRemaining());
    ASSERT_EQ(writer.remaining() == 0, reader.IsDoneReading());
  }
}
FUZZ_TEST(QuicheDataWriterFuzzTest, WriterAndReaderStayInSync)
    .WithDomains(
        /*buffer_size=*/InRange<size_t>(1u, 1024u * 1024u),
        /*endianness=*/
        ElementOf({quiche::NETWORK_BYTE_ORDER, quiche::HOST_BYTE_ORDER}),
        /*call_sequence=*/VectorOf(MethodFuzzUtil::CallVariantDomain()));

}  // namespace
}  // namespace quiche
