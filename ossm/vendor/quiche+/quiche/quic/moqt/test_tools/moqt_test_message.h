// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_MOQT_TEST_TOOLS_MOQT_TEST_MESSAGE_H_
#define QUICHE_QUIC_MOQT_TEST_TOOLS_MOQT_TEST_MESSAGE_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_data_reader.h"
#include "quiche/quic/core/quic_data_writer.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/common/moq_varint.h"
#include "quiche/common/platform/api/quiche_export.h"
#include "quiche/common/platform/api/quiche_logging.h"

namespace moqt::test {

inline constexpr absl::string_view kDefaultPropertyBlob(
    "\x00\x0c\x01\x03\x66\x6f\x6f", 7);

inline std::vector<MoqtDatagramType> AllMoqtDatagramTypes() {
  std::vector<MoqtDatagramType> types;
  for (bool payload : {false, true}) {
    for (bool property : {false, true}) {
      for (bool end_of_group : {false, true}) {
        for (bool default_priority : {false, true}) {
          for (bool zero_object_id : {false, true}) {
            types.push_back(MoqtDatagramType(payload, property, end_of_group,
                                             default_priority, zero_object_id));
          }
        }
      }
    }
  }
  return types;
}

inline std::vector<MoqtFetchSerialization> AllMoqtFetchSerializations() {
  std::vector<MoqtFetchSerialization> serializations;
  for (uint64_t i = 0; i < 128; ++i) {
    std::optional<MoqtFetchSerialization> value =
        MoqtFetchSerialization::FromValue(i);
    if (value.has_value()) {
      serializations.push_back(*value);
    } else {
      break;
    }
  }
  return serializations;
}

inline std::vector<MoqtDataStreamType> AllMoqtDataStreamTypes() {
  std::vector<MoqtDataStreamType> types;
  types.reserve(256);
  types.push_back(MoqtDataStreamType::Fetch());
  uint64_t first_object_id = 1;
  for (uint64_t subgroup_id : {0, 1, 2}) {
    for (bool no_properties : {true, false}) {
      for (bool default_priority : {true, false}) {
        for (bool has_first_object : {true, false}) {
          for (bool end_of_group : {false, true}) {
            types.push_back(MoqtDataStreamType::Subgroup(
                subgroup_id, first_object_id, no_properties, default_priority,
                has_first_object, end_of_group));
          }
        }
      }
    }
  }
  return types;
}

inline MessageParameters SubscribeForTest() {
  MessageParameters parameters;
  parameters.object_delivery_timeout =
      quic::QuicTimeDelta::FromMilliseconds(10000);
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "bar");
  parameters.rendezvous_timeout = quic::QuicTimeDelta::FromMilliseconds(100);
  parameters.set_forward(true);
  parameters.subscriber_priority = 0x20;
  parameters.location_filter =
      LocationFilter::AbsoluteStartFilter(Location(4, 1));
  parameters.group_order = MoqtGroupOrder::kDescending;
  return parameters;
}

constexpr absl::string_view kTestImplementationString =
    "Moq Test Implementation Type";

// Base class containing a wire image and the corresponding structured
// representation of an example of each message. It allows parser and framer
// tests to iterate through all message types without much specialized code.
class QUICHE_NO_EXPORT TestMessageBase {
 public:
  virtual ~TestMessageBase() = default;

  using MessageStructuredData = std::variant<
      MoqtSetup, MoqtObject, MoqtRequestOk, MoqtRequestError, MoqtSubscribe,
      MoqtSubscribeOk, MoqtPublishDone, MoqtRequestUpdate, MoqtPublishNamespace,
      MoqtTrackStatus, MoqtGoAway, MoqtSubscribeNamespace, MoqtSubscribeTracks,
      MoqtFetch, MoqtFetchOk, MoqtPublish, MoqtNamespace, MoqtNamespaceDone,
      MoqtPublishSkipped, MoqtPublishStateNotify, MoqtObjectAck>;

  // The total actual size of the message.
  size_t total_message_size() const { return wire_image_size_; }

  absl::string_view PacketSample() const {
    return absl::string_view(wire_image_, wire_image_size_);
  }

  void set_wire_image_size(size_t wire_image_size) {
    wire_image_size_ = wire_image_size;
  }

  // Returns a copy of the structured data for the message.
  virtual MessageStructuredData structured_data() const = 0;

  // Compares |values| to the derived class's structured data to make sure
  // they are equal.
  virtual bool EqualFieldValues(const MessageStructuredData& values) const = 0;

  // Expand all varints in the message. This is pure virtual because each
  // message has a different layout of varints.
  virtual void ExpandVarints() = 0;

  // This will cause a parsing error. Do not call this on Object Messages.
  void DecreasePayloadLengthByOne() {
    size_t length_offset =
        quiche::GetMoqVarintLengthForFirstByte(wire_image_[0]);
    wire_image_[length_offset + 1]--;
  }
  void IncreasePayloadLengthByOne() {
    size_t length_offset =
        quiche::GetMoqVarintLengthForFirstByte(wire_image_[0]);
    wire_image_[length_offset + 1]++;
    set_wire_image_size(wire_image_size_ + 1);
  }

  // Objects might need a different status if at the end of the stream.
  virtual void MakeObjectEndOfStream() {
    QUIC_LOG(INFO) << "MakeObjectEndOfStream not implemented";
  }

 protected:
  void SetWireImage(uint8_t* wire_image, size_t wire_image_size) {
    memcpy(wire_image_, wire_image, wire_image_size);
    wire_image_size_ = wire_image_size;
  }
  void SetByte(size_t offset, char value) { wire_image_[offset] = value; }

  // Expands all the varints in the message, alternating between making them 2,
  // 4, and 8 bytes long. Updates length fields accordingly.
  // Each character in |varints| corresponds to a byte in the original message
  // payload.
  // If there is a 'v', it is a varint that should be expanded. If '-', skip
  // to the next byte.
  void ExpandVarintsImpl(absl::string_view varints,
                         bool is_control_message = true) {
    int next_varint_len = 2;
    char new_wire_image[kMaxMessageHeaderSize + 1];
    quic::QuicDataReader reader(
        absl::string_view(wire_image_, wire_image_size_));
    quic::QuicDataWriter writer(sizeof(new_wire_image), new_wire_image);
    size_t length_field = 0;
    if (is_control_message) {
      std::optional<size_t> type_length_raw = reader.PeekMoqVarIntLength();
      uint8_t type_length =
          *type_length_raw;  // NOLINT(bugprone-unchecked-optional-access)
      uint64_t type;
      reader.ReadMoqVarInt(&type);
      if (type_length == 1) {
        // Expand the message type.
        type_length = next_varint_len;
        (void)quiche::WriteMoqVarintWithCustomLength(writer, type, type_length);
        next_varint_len = 4;
      } else {
        writer.WriteMoqVarInt(type);
      }
      length_field = writer.length();
      uint16_t size;
      reader.ReadUInt16(&size);
      writer.WriteUInt16(size);
    }
    size_t i = 0;
    while (!reader.IsDoneReading()) {
      if (i >= (varints.length()) || varints[i++] == '-') {
        uint8_t byte;
        reader.ReadUInt8(&byte);
        writer.WriteUInt8(byte);
        continue;
      }
      uint64_t value;
      reader.ReadMoqVarInt(&value);
      (void)quiche::WriteMoqVarintWithCustomLength(writer, value,
                                                   next_varint_len);
      next_varint_len *= 2;
      if (next_varint_len == 16) {
        next_varint_len = 2;
      }
    }
    memcpy(wire_image_, new_wire_image, writer.length());
    wire_image_size_ = writer.length();
    if (is_control_message) {  // First byte will be empty.
      quic::QuicDataWriter length_writer(writer.length(),
                                         &wire_image_[length_field]);
      length_writer.WriteUInt16(writer.length() - length_field - 2);
    }
  }

 private:
  char wire_image_[kMaxMessageHeaderSize + 20];
  size_t wire_image_size_;
};

// Base class for the two subtypes of Object Message.
class QUICHE_NO_EXPORT ObjectMessage : public TestMessageBase {
 public:
  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::move(std::get<MoqtObject>(values));
    if (cast.track_alias != object_.track_alias) {
      QUIC_LOG(INFO) << "OBJECT Track ID mismatch";
      return false;
    }
    if (cast.group_id != object_.group_id) {
      QUIC_LOG(INFO) << "OBJECT Group Sequence mismatch";
      return false;
    }
    if (cast.object_id != object_.object_id) {
      QUIC_LOG(INFO) << "OBJECT Object Sequence mismatch";
      return false;
    }
    if (cast.publisher_priority != object_.publisher_priority) {
      QUIC_LOG(INFO) << "OBJECT Publisher Priority mismatch";
      return false;
    }
    if (cast.properties != object_.properties) {
      QUIC_LOG(INFO) << "OBJECT Property Header mismatch";
      return false;
    }
    if (cast.object_status != object_.object_status) {
      QUIC_LOG(INFO) << "OBJECT Object Status mismatch";
      return false;
    }
    if (cast.subgroup_id != object_.subgroup_id) {
      QUIC_LOG(INFO) << "OBJECT Subgroup ID mismatch";
      return false;
    }
    if (cast.payload_length != object_.payload_length) {
      QUIC_LOG(INFO) << "OBJECT Payload Length mismatch";
      return false;
    }
    return true;
  }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(object_);
  }

 protected:
  MoqtObject object_{
      .track_alias = 4,
      .group_id = 5,
      .object_id = 6,
      .publisher_priority = 7,
      .properties = std::string(kDefaultPropertyBlob),
      .object_status = MoqtObjectStatus::kNormal,
      .subgroup_id = 8,
      .first_object_in_subgroup = false,
      .payload_length = 3,
  };
};

class QUICHE_NO_EXPORT ObjectDatagramMessage : public ObjectMessage {
 public:
  ObjectDatagramMessage(MoqtDatagramType datagram_type)
      : ObjectMessage(), datagram_type_(datagram_type) {
    // Update ObjectMessage::object_ to match the datagram type.
    if (datagram_type.has_status()) {
      object_.object_status = datagram_type.has_properties()
                                  ? MoqtObjectStatus::kNormal
                                  : MoqtObjectStatus::kEndOfTrack;
      object_.payload_length = 0;
    } else {
      object_.object_status = datagram_type.end_of_group()
                                  ? MoqtObjectStatus::kEndOfGroup
                                  : MoqtObjectStatus::kNormal;
      object_.payload_length = 3;
    }
    object_.properties =
        datagram_type.has_properties() ? std::string(kDefaultPropertyBlob) : "";
    object_.object_id = datagram_type.has_object_id() ? 6 : 0;
    object_.subgroup_id = std::nullopt;
    quic::QuicDataWriter writer(sizeof(raw_packet_),
                                reinterpret_cast<char*>(raw_packet_));
    EXPECT_TRUE(writer.WriteMoqVarInt(datagram_type.value()));
    EXPECT_TRUE(writer.WriteStringPiece(kRawAliasGroup));
    if (datagram_type.has_object_id()) {
      EXPECT_TRUE(writer.WriteStringPiece(kRawObject));
    }
    if (!datagram_type.has_default_priority()) {
      EXPECT_TRUE(writer.WriteStringPiece(kRawPriority));
    }
    if (datagram_type.has_properties()) {
      EXPECT_TRUE(writer.WriteStringPiece(kRawProperties));
    }
    if (datagram_type.has_status()) {
      EXPECT_TRUE(
          writer.WriteMoqVarInt(static_cast<uint64_t>(object_.object_status)));
    } else {
      EXPECT_TRUE(writer.WriteStringPiece(kRawPayload));
    }
    EXPECT_LE(writer.length(), kMaxMessageHeaderSize);
    SetWireImage(raw_packet_, writer.length());
  }

  void ExpandVarints() override {
    std::string varints = "vvv";
    if (datagram_type_.has_object_id()) {
      varints += "v";
    }
    if (!datagram_type_.has_default_priority()) {
      varints += "-";  // priority
    }
    if (datagram_type_.has_properties()) {
      varints += "v-------";
    }
    if (datagram_type_.has_status()) {
      varints += "v";
    }
    ExpandVarintsImpl(varints, false);
  }

  MoqtPriority publisher_priority() const { return object_.publisher_priority; }

 private:
  uint8_t raw_packet_[17];
  MoqtDatagramType datagram_type_;
  static constexpr absl::string_view kRawAliasGroup = "\x04\x05";
  static constexpr absl::string_view kRawObject = "\x06";
  static constexpr absl::string_view kRawPriority = "\x07";
  static constexpr absl::string_view kRawProperties{
      "\x07\x00\x0c\x01\x03\x66\x6f\x6f", 8};  // see kDefaultPropertyBlob
  static constexpr absl::string_view kRawPayload = "foo";
};

// Concatenation of the base header and the object-specific header. Follow-on
// object headers are handled in a different class.
class QUICHE_NO_EXPORT StreamHeaderSubgroupMessage : public ObjectMessage {
 public:
  explicit StreamHeaderSubgroupMessage(MoqtDataStreamType type)
      : ObjectMessage(), type_(type) {
    // Update ObjectMessage from the type;
    if (type.SubgroupIsZero()) {
      object_.subgroup_id = 0;
    } else if (type.SubgroupIsFirstObjectId()) {
      object_.subgroup_id = object_.object_id;
    }
    if (!type.ArePropertiesPresent()) {
      object_.properties = "";
    }
    // Build raw_packet_ from the type.
    quic::QuicDataWriter writer(sizeof(raw_packet_), raw_packet_);
    EXPECT_TRUE(
        writer.WriteMoqVarInt(type.value()) &&
        writer.WriteBytes(kRawBeginning.data(), kRawBeginning.length()));
    if (type.IsSubgroupPresent()) {
      EXPECT_TRUE(
          writer.WriteBytes(kRawSubgroupId.data(), kRawSubgroupId.length()));
    }
    if (!type.HasDefaultPriority()) {
      EXPECT_TRUE(writer.WriteBytes(kRawPublisherPriority.data(),
                                    kRawPublisherPriority.length()));
    }
    EXPECT_TRUE(writer.WriteBytes(kRawObjectId.data(), kRawObjectId.length()));
    if (type.ArePropertiesPresent()) {
      EXPECT_TRUE(
          writer.WriteBytes(kRawProperties.data(), kRawProperties.length()));
    }
    payload_length_offset_ = writer.length();
    EXPECT_TRUE(writer.WriteBytes(kRawPayload.data(), kRawPayload.length()));
    EXPECT_LE(writer.length(), kMaxMessageHeaderSize);
    SetWireImage(reinterpret_cast<uint8_t*>(raw_packet_), writer.length());
  }

  void ExpandVarints() override {
    std::string varints = "vvv";
    if (type_.IsSubgroupPresent()) {
      varints += "v";
    }
    if (!type_.HasDefaultPriority()) {
      varints += "-";  // priority
    }
    varints += "v";  // object ID
    if (type_.ArePropertiesPresent()) {
      varints += "v-------";
    }
    varints += "v---";  // payload with length
    ExpandVarintsImpl(varints, false);
  }

  bool SetPayloadLength(uint8_t payload_length) {
    if (payload_length > 63) {
      // This only supports one-byte varints.
      return false;
    }
    int payload_length_change = payload_length - object_.payload_length;
    object_.payload_length = payload_length;
    raw_packet_[payload_length_offset_] = static_cast<char>(payload_length);
    SetWireImage(reinterpret_cast<uint8_t*>(raw_packet_), total_message_size());
    set_wire_image_size(total_message_size() + payload_length_change);
    return true;
  }

  void MakeObjectEndOfStream() override {
    if (type_.EndOfGroupInStream()) {
      object_.object_status = MoqtObjectStatus::kEndOfGroup;
    }
  }

 private:
  MoqtDataStreamType type_;
  static constexpr absl::string_view kRawBeginning = "\x04\x05";
  // track alias, group ID
  static constexpr absl::string_view kRawSubgroupId = "\x08";
  static constexpr absl::string_view kRawPublisherPriority = "\x07";
  static constexpr absl::string_view kRawObjectId = "\x06";
  static constexpr absl::string_view kRawProperties{
      "\x07\x00\x0c\x01\x03\x66\x6f\x6f", 8};  // see kDefaultPropertyBlob
  static constexpr absl::string_view kRawPayload = "\x03\x66\x6f\x6f";
  char raw_packet_[18];
  size_t payload_length_offset_;
};

// Used only for tests that process multiple objects on one stream.
class QUICHE_NO_EXPORT StreamMiddlerSubgroupMessage : public ObjectMessage {
 public:
  StreamMiddlerSubgroupMessage(const MoqtDataStreamType type)
      : ObjectMessage(), type_(type) {
    SetWireImage(reinterpret_cast<uint8_t*>(raw_packet_), sizeof(raw_packet_));
    if (type.SubgroupIsZero()) {
      object_.subgroup_id = 0;
    } else if (type.SubgroupIsFirstObjectId()) {
      object_.subgroup_id = 6;  // The object ID in the header.
    }
    object_.object_id = 9;
    quic::QuicDataWriter writer(sizeof(raw_packet_), raw_packet_);
    EXPECT_TRUE(writer.WriteMoqVarInt(2));  // Object ID delta - 1
    if (type.ArePropertiesPresent()) {
      EXPECT_TRUE(
          writer.WriteBytes(kRawProperties.data(), kRawProperties.length()));
    }
    EXPECT_TRUE(writer.WriteBytes(kRawPayload.data(), kRawPayload.length()));
    EXPECT_LE(writer.length(), kMaxMessageHeaderSize);
    SetWireImage(reinterpret_cast<uint8_t*>(raw_packet_), writer.length());
  }

  void ExpandVarints() override {
    if (type_.ArePropertiesPresent()) {
      ExpandVarintsImpl("vv-------v---", false);
    } else {
      ExpandVarintsImpl("vv---", false);
    }
  }

 private:
  MoqtDataStreamType type_;
  static constexpr absl::string_view kRawProperties{
      "\x07\x00\x0c\x01\x03\x66\x6f\x6f", 8};  // see kDefaultPropertyBlob
  static constexpr absl::string_view kRawPayload = "\x03\x62\x61\x72";
  char raw_packet_[13];
};

class QUICHE_NO_EXPORT StreamHeaderFetchMessage : public ObjectMessage {
 public:
  StreamHeaderFetchMessage() : ObjectMessage() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
    object_.subgroup_id = 8;
  }

  void ExpandVarints() override {
    ExpandVarintsImpl("vvvvvv-v-------v---", false);
  }

  bool SetPayloadLength(uint8_t payload_length) {
    if (payload_length > 63) {
      // This only supports one-byte varints.
      return false;
    }
    object_.payload_length = payload_length;
    raw_packet_[14] = payload_length;
    SetWireImage(raw_packet_, sizeof(raw_packet_));
    return true;
  }

 private:
  uint8_t raw_packet_[19] = {
      0x05,              // type field
      0x04,              // request ID
      0x3f,              // object serialization flag
      0x05, 0x08, 0x06,  // sequence
      0x07, 0x07,        // publisher priority, 7B properties
      0x00, 0x0c, 0x01, 0x03, 0x66, 0x6f, 0x6f,  // properties
      0x03, 0x66, 0x6f, 0x6f,                    // payload = "foo"
  };
};

// Used only for tests that process multiple objects on one stream.
class QUICHE_NO_EXPORT StreamMiddlerFetchMessage : public ObjectMessage {
 public:
  StreamMiddlerFetchMessage(MoqtFetchSerialization serialization)
      : ObjectMessage(), serialization_(serialization) {
    size_t length = 0;
    raw_packet_[length++] = static_cast<uint8_t>(serialization.value());
    if (serialization.has_group_id()) {
      raw_packet_[length++] = 0x00;  // group ID delta = 0 (group ID = 6)
      object_.group_id = 6;
    }
    if (serialization.zero_subgroup_id()) {
      object_.subgroup_id = 0;
    } else if (serialization.has_subgroup_id()) {
      raw_packet_[length++] = 0x0a;
      object_.subgroup_id = 10;
    } else if (serialization.prior_subgroup_id_plus_one()) {
      if (!object_.subgroup_id.has_value()) {
        QUICHE_BUG(quiche_bug_moqt_prior_subgroup_id_without_previous_subgroup)
            << "prior_subgroup_id_plus_one without previous subgroup ID";
        return;
      }
      ++(*object_.subgroup_id);
    } else if (serialization.is_datagram()) {
      object_.subgroup_id = std::nullopt;
    }  // If prior_subgroup_id, subgroup_id is already set properly.
    if (serialization.has_object_id()) {
      if (serialization.has_group_id()) {
        raw_packet_[length++] = 0x0a;  // explicit object ID = 10
      } else {
        raw_packet_[length++] = 0x04;  // object ID delta = 4 (object ID = 10)
      }
      object_.object_id = 10;
    } else {
      ++object_.object_id;
    }
    if (serialization.has_priority()) {
      raw_packet_[length++] = 0x09;
      object_.publisher_priority = MoqtPriority(0x09);
    }
    if (serialization.has_properties()) {
      memcpy(&raw_packet_[length], kRawProperties.data(),
             kRawProperties.length());
      length += kRawProperties.length();
    } else {
      object_.properties = "";
    }
    memcpy(&raw_packet_[length], kRawPayload.data(), kRawPayload.length());
    length += kRawPayload.length();

    SetWireImage(raw_packet_, length);
  }

  void ExpandVarints() override {
    std::string varints = "v";
    if (serialization_.has_group_id()) {
      varints += "v";
    }
    if (serialization_.has_subgroup_id()) {
      varints += "v";
    }
    if (serialization_.has_object_id()) {
      varints += "v";
    }
    if (serialization_.has_priority()) {
      varints += "-";
    }
    if (serialization_.has_properties()) {
      varints += "v-------";
    }
    varints += "v---";
    ExpandVarintsImpl(varints, false);
  }

 private:
  MoqtFetchSerialization serialization_;
  uint8_t raw_packet_[17];
  static constexpr absl::string_view kRawProperties{
      "\x07\x00\x0c\x01\x03\x66\x6f\x6f", 8};  // see kDefaultPropertyBlob
  static constexpr absl::string_view kRawPayload = "\x03\x62\x61\x72";
};

class QUICHE_NO_EXPORT ClientSetupMessage : public TestMessageBase {
 public:
  explicit ClientSetupMessage(bool webtrans) : TestMessageBase() {
    client_setup_.options.moqt_implementation = kTestImplementationString;
    if (webtrans) {
      // Should not send PATH or AUTHORITY.
      client_setup_.options.path = std::nullopt;
      client_setup_.options.authority = std::nullopt;
      raw_packet_[3] -= 17;  // adjust payload length
      // Move MoqtImplementation up in the packet.
      memmove(raw_packet_ + 4, raw_packet_ + 21,
              kTestImplementationString.length() + 2);
      raw_packet_[4] = 0x07;  // Diff from 0.
      SetWireImage(raw_packet_, sizeof(raw_packet_) - 17);
    } else {
      SetWireImage(raw_packet_, sizeof(raw_packet_));
    }
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtSetup>(values);
    if (cast.options != client_setup_.options) {
      QUIC_LOG(INFO) << "CLIENT_SETUP option mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override {
    if (client_setup_.options.path.has_value()) {
      ExpandVarintsImpl("vv----vv---------vv---------------------------");
    } else {
      ExpandVarintsImpl("vv---------------------------");
    }
  }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(client_setup_);
  }

 private:
  // The framer serializes all the integer options in order, then all the
  // string options in order. Unfortunately, this means that
  // kMoqtImplementation goes last even though it is always present, while
  // kPath and KAuthority aren't.
  uint8_t raw_packet_[51] = {
      0xaf, 0x00, 0x00, 0x2f,              // type, length
      0x01, 0x04, 0x70, 0x61, 0x74, 0x68,  // path = "path"
      0x04, 0x09, 0x61, 0x75, 0x74, 0x68, 0x6f, 0x72, 0x69, 0x74,
      0x79,  // authority = "authority"
      // moqt_implementation:
      0x02, 0x1c, 0x4d, 0x6f, 0x71, 0x20, 0x54, 0x65, 0x73, 0x74, 0x20, 0x49,
      0x6d, 0x70, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x74, 0x61, 0x74, 0x69, 0x6f,
      0x6e, 0x20, 0x54, 0x79, 0x70, 0x65};
  MoqtSetup client_setup_ = {
      SetupOptions("path", "authority"),
  };
};

class QUICHE_NO_EXPORT ServerSetupMessage : public TestMessageBase {
 public:
  ServerSetupMessage() : TestMessageBase() {
    server_setup_.options.moqt_implementation = kTestImplementationString;
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtSetup>(values);
    if (cast.options != server_setup_.options) {
      QUIC_LOG(INFO) << "SERVER_SETUP option mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vv"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(server_setup_);
  }

 private:
  uint8_t raw_packet_[34] = {0xaf, 0x00, 0x00, 0x1e,  // type, length
                                                      // moqt_implementation:
                             0x07, 0x1c, 0x4d, 0x6f, 0x71, 0x20, 0x54, 0x65,
                             0x73, 0x74, 0x20, 0x49, 0x6d, 0x70, 0x6c, 0x65,
                             0x6d, 0x65, 0x6e, 0x74, 0x61, 0x74, 0x69, 0x6f,
                             0x6e, 0x20, 0x54, 0x79, 0x70, 0x65};
  MoqtSetup server_setup_ = {
      SetupOptions(),
  };
};

class QUICHE_NO_EXPORT SubscribeMessage : public TestMessageBase {
 public:
  SubscribeMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtSubscribe>(values);
    if (cast.request_id != subscribe_.request_id) {
      QUIC_LOG(INFO) << "SUBSCRIBE subscribe ID mismatch";
      return false;
    }
    if (cast.full_track_name != subscribe_.full_track_name) {
      QUIC_LOG(INFO) << "SUBSCRIBE track name mismatch";
      return false;
    }
    if (cast.parameters != subscribe_.parameters) {
      return false;
    }
    return true;
  }

  void ExpandVarints() override {
    ExpandVarintsImpl("vvv---v----vv--vv-----vvv-v-vvvvv-");
  }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(subscribe_);
  }

 protected:
  MoqtSubscribe subscribe_ = {
      /*request_id=*/1,
      FullTrackName("foo", "abcd"),
      SubscribeForTest(),
  };

 private:
  uint8_t raw_packet_[37] = {
      0x03, 0x00, 0x22, 0x01,        // request_id = 1
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x07,                          // 7 parameters
      0x02, 0xa7, 0x10,              // object_delivery_timeout = 10000 ms
      0x01, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization_tag = "bar"
      0x01, 0x64,                                // rendezvous_timeout = 100 ms
      0x0c, 0x01,                                // forward = true
      0x10, 0x20,                                // subscriber_priority = 0x20
      0x01, 0x02, 0x04, 0x01,  // filter_type = kAbsoluteStart (4, 1)
      0x01, 0x02,              // group_order = kDescending
  };
};

class QUICHE_NO_EXPORT SubscribeOkMessage : public TestMessageBase {
 public:
  SubscribeOkMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
    subscribe_ok_.parameters.expires = quic::QuicTimeDelta::FromMilliseconds(3);
    subscribe_ok_.parameters.largest_object = Location(12, 20);
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtSubscribeOk>(values);
    if (cast.track_alias != subscribe_ok_.track_alias) {
      QUIC_LOG(INFO) << "SUBSCRIBE OK track alias mismatch";
      return false;
    }
    if (cast.parameters != subscribe_ok_.parameters) {
      QUIC_LOG(INFO) << "SUBSCRIBE OK parameter mismatch";
      return false;
    }
    if (cast.properties != subscribe_ok_.properties) {
      QUIC_LOG(INFO) << "SUBSCRIBE OK properties mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vvvvvvvv--v--vv"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(subscribe_ok_);
  }

  void SetInvalidDeliveryOrder() {
    raw_packet_[17] = 0x10;
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

 protected:
  // This is protected so that TrackStatusOk can edit the fields.
  MoqtSubscribeOk subscribe_ok_ = {
      /*track_alias=*/2,
      MessageParameters(),  // Set in the constructor.
      TrackProperties(
          /*object_delivery_timeout=*/
          quic::QuicTimeDelta::FromMilliseconds(10000),
          /*max_cache_duration=*/quic::QuicTimeDelta::FromMilliseconds(10000),
          /*subgroup_delivery_timeout=*/std::nullopt,
          /*publisher_priority=*/std::nullopt,
          /*group_order=*/MoqtGroupOrder::kDescending,
          /*dynamic_groups=*/std::nullopt,
          /*immutable_properties=*/std::nullopt),
  };

 private:
  uint8_t raw_packet_[18] = {
      0x04, 0x00, 0x0f, 0x02, 0x02,  // alias, 2 params
      0x08, 0x03,                    // expires = 3
      0x01, 0x0c, 0x14,              // largest_location = (12, 20)
      // Properties
      0x02, 0xa7, 0x10,  // object_delivery_timeout = 10000
      0x02, 0xa7, 0x10,  // max_cache_duration = 10000
      0x1e, 0x02         // default_publisher_group_order = 2
  };
};

class QUICHE_NO_EXPORT RequestErrorMessage : public TestMessageBase {
 public:
  RequestErrorMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtRequestError>(values);
    if (cast.error_code != request_error_.error_code) {
      QUIC_LOG(INFO) << "REQUEST_ERROR error code mismatch";
      return false;
    }
    if (cast.retry_interval != request_error_.retry_interval) {
      QUIC_LOG(INFO) << "REQUEST_ERROR retry interval mismatch";
      return false;
    }
    if (cast.reason_phrase != request_error_.reason_phrase) {
      QUIC_LOG(INFO) << "REQUEST_ERROR reason phrase mismatch";
      return false;
    }
    if (cast.redirect != request_error_.redirect) {
      QUIC_LOG(INFO) << "REQUEST_ERROR redirect mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vv---"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(request_error_);
  }

 protected:
  MoqtRequestError request_error_ = {
      /*error_code=*/RequestErrorCode::kInvalidRange,
      /*retry_interval=*/quic::QuicTimeDelta::FromSeconds(10),
      /*reason_phrase=*/"bar",
  };

 private:
  uint8_t raw_packet_[10] = {
      0x05, 0x00, 0x07,
      0x11,                    // error_code = 17
      0xa7, 0x11,              // retry_interval = 10000 ms
      0x03, 0x62, 0x61, 0x72,  // reason_phrase = "bar"
  };
};


class QUICHE_NO_EXPORT PublishDoneMessage : public TestMessageBase {
 public:
  PublishDoneMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtPublishDone>(values);
    if (cast.status_code != publish_done_.status_code) {
      QUIC_LOG(INFO) << "PUBLISH_DONE status code mismatch";
      return false;
    }
    if (cast.stream_count != publish_done_.stream_count) {
      QUIC_LOG(INFO) << "PUBLISH_DONE stream count mismatch";
      return false;
    }
    if (cast.error_reason != publish_done_.error_reason) {
      QUIC_LOG(INFO) << "PUBLISH_DONE error reason mismatch";
      return false;
    }

    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vvv--"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(publish_done_);
  }

 private:
  uint8_t raw_packet_[8] = {
      0x0b, 0x00, 0x05, 0x02,  // status_code = 2
      0x05,                    // stream_count = 5
      0x02, 0x68, 0x69,        // error_reason = "hi"
  };

  MoqtPublishDone publish_done_ = {
      /*status_code=*/PublishDoneCode::kTrackEnded,
      /*stream_count=*/5,
      /*error_reason=*/"hi",
  };
};

class QUICHE_NO_EXPORT RequestUpdateMessage : public TestMessageBase {
 public:
  RequestUpdateMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
    request_update_.parameters.object_delivery_timeout =
        quic::QuicTimeDelta::FromMilliseconds(10000);
    request_update_.parameters.set_forward(true);
    request_update_.parameters.subscriber_priority = 0xaa;
    request_update_.parameters.location_filter =
        LocationFilter::AbsoluteStartGroupEndFilter(Location(3, 1), 5);
    request_update_.parameters.track_namespace_prefix = TrackNamespace({"bar"});
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtRequestUpdate>(values);
    if (cast.request_id != request_update_.request_id) {
      QUIC_LOG(INFO) << "REQUEST_UPDATE request ID mismatch";
      return false;
    }
    if (cast.parameters != request_update_.parameters) {
      QUIC_LOG(INFO) << "REQUEST_UPDATE parameter mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vvv--v-v-vvvvvvvv---"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(request_update_);
  }

 private:
  uint8_t raw_packet_[23] = {
      0x02, 0x00, 0x14, 0x02,              // request_id = 2
      0x05,                                // Five parameters
      0x02, 0xa7, 0x10,                    // object_delivery_timeout = 10000
      0x0e, 0x01,                          // forward = true
      0x10, 0xaa,                          // subscriber_priority = 0xaa
      0x01, 0x03, 0x03, 0x01, 0x02,        // AbsoluteStartGroupEnd: (3, 1) to 5
      0x13, 0x01, 0x03, 0x62, 0x61, 0x72,  // prefix = "bar"
  };

  MoqtRequestUpdate request_update_ = {
      /*request_id=*/2,
      MessageParameters(),  // Set in the constructor.
  };
};

class QUICHE_NO_EXPORT PublishNamespaceMessage : public TestMessageBase {
 public:
  PublishNamespaceMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
    publish_namespace_.parameters.authorization_tokens.push_back(
        AuthToken(AuthTokenType::kOutOfBand, "bar"));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtPublishNamespace>(values);
    if (cast.request_id != publish_namespace_.request_id) {
      QUIC_LOG(INFO) << "PUBLISH_NAMESPACE request ID mismatch";
      return false;
    }
    if (cast.track_namespace != publish_namespace_.track_namespace) {
      QUIC_LOG(INFO) << "PUBLISH_NAMESPACE track namespace mismatch";
      return false;
    }
    if (cast.parameters != publish_namespace_.parameters) {
      QUIC_LOG(INFO) << "PUBLISH_NAMESPACE parameter mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vvv---vvv-----"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(publish_namespace_);
  }

 private:
  uint8_t raw_packet_[17] = {
      0x06, 0x00, 0x0e, 0x02,                    // request_id = 2
      0x01, 0x03, 0x66, 0x6f, 0x6f,              // track_namespace = "foo"
      0x01,                                      // 1 parameter
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization_tag = "bar"
  };

  MoqtPublishNamespace publish_namespace_ = {
      /*request_id=*/2,
      TrackNamespace{"foo"},
      MessageParameters(),
  };
};

class QUICHE_NO_EXPORT NamespaceMessage : public TestMessageBase {
 public:
  NamespaceMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtNamespace>(values);
    if (cast.track_namespace_suffix != namespace_.track_namespace_suffix) {
      QUIC_LOG(INFO) << "NAMESPACE suffix mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vv---"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(namespace_);
  }

 private:
  uint8_t raw_packet_[8] = {
      0x08, 0x00, 0x05, 0x01,
      0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
  };

  MoqtNamespace namespace_ = {
      TrackNamespace{"foo"},
  };
};

class QUICHE_NO_EXPORT NamespaceDoneMessage : public TestMessageBase {
 public:
  NamespaceDoneMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtNamespaceDone>(values);
    if (cast.track_namespace_suffix != namespace_done_.track_namespace_suffix) {
      QUIC_LOG(INFO) << "NAMESPACE_DONE suffix mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vv---"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(namespace_done_);
  }

 private:
  uint8_t raw_packet_[8] = {
      0x0e, 0x00, 0x05, 0x01,
      0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
  };

  MoqtNamespaceDone namespace_done_ = {
      TrackNamespace{"foo"},
  };
};

class QUICHE_NO_EXPORT PublishSkippedMessage : public TestMessageBase {
 public:
  PublishSkippedMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtPublishSkipped>(values);
    if (cast.name != publish_skipped_.name) {
      QUIC_LOG(INFO) << "PUBLISH_SKIPPED name mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vv---v---"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(publish_skipped_);
  }

 private:
  uint8_t raw_packet_[12] = {
      0x0f, 0x00, 0x09, 0x01,
      0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x03, 0x62, 0x61, 0x72,  // track_name = "bar"
  };

  MoqtPublishSkipped publish_skipped_ = {
      FullTrackName("foo", "bar"),
  };
};

class QUICHE_NO_EXPORT RequestOkMessage : public TestMessageBase {
 public:
  RequestOkMessage() : TestMessageBase() {
    request_ok_.parameters.largest_object = Location(5, 1);
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtRequestOk>(values);
    if (cast.parameters != request_ok_.parameters) {
      QUIC_LOG(INFO) << "REQUEST_OK parameter mismatch";
      return false;
    }
    if (cast.properties != request_ok_.properties) {
      QUIC_LOG(INFO) << "REQUEST_OK properties mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vvvvv--v--v-"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(request_ok_);
  }

 private:
  uint8_t raw_packet_[15] = {
      0x07, 0x00, 0x0c,
      0x01,              // 1 parameter
      0x09, 0x05, 0x01,  // Largest Object = (5, 1)
      // Properties
      0x02, 0xa7, 0x10,  // object_delivery_timeout = 10000
      0x02, 0xa7, 0x10,  // max_cache_duration = 10000
      0x1e, 0x02         // default_publisher_group_order = 2
  };

  MoqtRequestOk request_ok_ = {
      MessageParameters(),  // Set in the constructor.
      TrackProperties(
          /*object_delivery_timeout=*/
          quic::QuicTimeDelta::FromMilliseconds(10000),
          /*max_cache_duration=*/quic::QuicTimeDelta::FromMilliseconds(10000),
          /*subgroup_delivery_timeout=*/std::nullopt,
          /*publisher_priority=*/std::nullopt,
          /*group_order=*/MoqtGroupOrder::kDescending,
          /*dynamic_groups=*/std::nullopt,
          /*immutable_properties=*/std::nullopt),
  };
};

class QUICHE_NO_EXPORT TrackStatusMessage : public TestMessageBase {
 public:
  TrackStatusMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
    track_status_.parameters = MessageParameters();
    track_status_.parameters.authorization_tokens.emplace_back(
        AuthTokenType::kOutOfBand, "bar");
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtTrackStatus>(values);
    if (cast.request_id != track_status_.request_id) {
      QUIC_LOG(INFO) << "TRACK_STATUS request_id mismatch";
      return false;
    }
    if (cast.full_track_name != track_status_.full_track_name) {
      QUIC_LOG(INFO) << "TRACK_STATUS full_track_name mismatch";
      return false;
    }
    if (cast.parameters != track_status_.parameters) {
      QUIC_LOG(INFO) << "TRACK_STATUS parameters mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vvv---v----vvv-----"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(track_status_);
  }

 private:
  MoqtTrackStatus track_status_ = {
      /*request_id=*/1,
      FullTrackName("foo", "abcd"),
      MessageParameters(),  // Set in the constructor.
  };

  uint8_t raw_packet_[22] = {
      0x0d, 0x00, 0x13, 0x01,                    // request_id = 1
      0x01, 0x03, 0x66, 0x6f, 0x6f,              // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,              // track_name = "abcd"
      0x01,                                      // 1 parameter
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization_tag = "bar"
  };
};

class QUICHE_NO_EXPORT GoAwayMessage : public TestMessageBase {
 public:
  GoAwayMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtGoAway>(values);
    if (cast.new_session_uri != goaway_.new_session_uri) {
      QUIC_LOG(INFO) << "GOAWAY new session URI mismatch";
      return false;
    }
    if (cast.timeout != goaway_.timeout) {
      QUIC_LOG(INFO) << "GOAWAY timeout mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("v---v"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(goaway_);
  }

 private:
  uint8_t raw_packet_[8] = {
      0x10, 0x00, 0x05, 0x03, 0x66, 0x6f, 0x6f, 0x03,
  };

  MoqtGoAway goaway_ = {
      /*new_session_uri=*/"foo",
      /*timeout=*/quic::QuicTimeDelta::FromMilliseconds(3),
  };
};

class QUICHE_NO_EXPORT SubscribeNamespaceMessage : public TestMessageBase {
 public:
  SubscribeNamespaceMessage() : TestMessageBase() {
    subscribe_namespace_.parameters.authorization_tokens.push_back(
        AuthToken(AuthTokenType::kOutOfBand, "bar"));
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtSubscribeNamespace>(values);
    if (cast.request_id != subscribe_namespace_.request_id) {
      QUIC_LOG(INFO) << "SUBSCRIBE_NAMESPACE request_id mismatch";
      return false;
    }
    if (cast.track_namespace_prefix !=
        subscribe_namespace_.track_namespace_prefix) {
      QUIC_LOG(INFO) << "SUBSCRIBE_NAMESPACE track namespace mismatch";
      return false;
    }
    if (cast.parameters != subscribe_namespace_.parameters) {
      QUIC_LOG(INFO) << "SUBSCRIBE_NAMESPACE parameters mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vvv---vvv-----"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(subscribe_namespace_);
  }

 private:
  uint8_t raw_packet_[17] = {
      0x50, 0x00, 0x0e, 0x01,                    // request_id = 1
      0x01, 0x03, 0x66, 0x6f, 0x6f,              // namespace = "foo"
      0x01,                                      // 1 parameter
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization_tag = "bar"
  };

  MoqtSubscribeNamespace subscribe_namespace_ = {
      /*request_id=*/1,
      TrackNamespace({"foo"}),
      MessageParameters(),  // set in constructor.
  };
};

class QUICHE_NO_EXPORT SubscribeTracksMessage : public TestMessageBase {
 public:
  SubscribeTracksMessage() : TestMessageBase() {
    subscribe_tracks_.parameters.authorization_tokens.push_back(
        AuthToken(AuthTokenType::kOutOfBand, "bar"));
    subscribe_tracks_.parameters.set_forward(true);
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtSubscribeTracks>(values);
    if (cast.request_id != subscribe_tracks_.request_id) {
      QUIC_LOG(INFO) << "SUBSCRIBE_TRACKS request_id mismatch";
      return false;
    }
    if (cast.track_namespace_prefix !=
        subscribe_tracks_.track_namespace_prefix) {
      QUIC_LOG(INFO) << "SUBSCRIBE_TRACKS track namespace mismatch";
      return false;
    }
    if (cast.parameters != subscribe_tracks_.parameters) {
      QUIC_LOG(INFO) << "SUBSCRIBE_TRACKS parameters mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vvv---vvv-----v-"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(subscribe_tracks_);
  }

 private:
  uint8_t raw_packet_[19] = {
      0x51, 0x00, 0x10, 0x01,                    // request_id = 1
      0x01, 0x03, 0x66, 0x6f, 0x6f,              // namespace = "foo"
      0x02,                                      // 2 parameters
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization_tag = "bar"
      0x0d, 0x01,                                // forward = true
  };

  MoqtSubscribeTracks subscribe_tracks_ = {
      /*request_id=*/1,
      TrackNamespace({"foo"}),
      MessageParameters(),  // set in constructor.
  };
};

class QUICHE_NO_EXPORT FetchMessage : public TestMessageBase {
 public:
  FetchMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
    fetch_.parameters.authorization_tokens.push_back(
        AuthToken(AuthTokenType::kOutOfBand, "baz"));
    fetch_.parameters.group_order = MoqtGroupOrder::kAscending;
    fetch_.parameters.fill_timeout = quic::QuicTimeDelta::FromMilliseconds(100);
    fetch_.parameters.subscriber_priority = 2;
  }
  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtFetch>(values);
    if (cast.request_id != fetch_.request_id) {
      QUIC_LOG(INFO) << "FETCH request_id mismatch";
      return false;
    }
    if (cast.fetch != fetch_.fetch) {
      QUIC_LOG(INFO) << "FETCH mismatch";
      return false;
    }
    if (cast.parameters != fetch_.parameters) {
      QUIC_LOG(INFO) << "FETCH parameters mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override {
    ExpandVarintsImpl("vvvv---v---vvvvvvv-----vvv-v-");
  }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(fetch_);
  }

  void SetEndObject(uint64_t group, std::optional<uint64_t> object) {
    // Avoid varint nonsense.
    QUICHE_CHECK(group < 64);
    QUICHE_CHECK(!object.has_value() || *object < 64);
    std::get<StandaloneFetch>(fetch_.fetch).end_location.group = group;
    std::get<StandaloneFetch>(fetch_.fetch).end_location.object =
        object.has_value() ? *object : kMaxObjectId;
    raw_packet_[16] = group;
    raw_packet_[17] = object.has_value() ? (*object + 1) : 0;
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  void SetGroupOrder(uint8_t group_order) {
    raw_packet_[29] = static_cast<uint8_t>(group_order);
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

 private:
  uint8_t raw_packet_[32] = {
      0x16, 0x00, 0x1d,
      0x01,                                      // request_id = 1
      0x01,                                      // type = kStandalone
      0x01, 0x03, 0x66, 0x6f, 0x6f,              // track_namespace = "foo"
      0x03, 0x62, 0x61, 0x72,                    // track_name = "bar"
      0x01, 0x02,                                // start_location = 1, 2
      0x05, 0x07,                                // end_location = 5, 6
      0x04,                                      // 3 parameters
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x7a,  // token = "baz"
      0x07, 0x64,                                // fill_timeout = 100ms
      0x16, 0x02,                                // priority = kHigh
      0x02, 0x01,                                // group_order = kAscending
  };

  MoqtFetch fetch_ = {
      /*request_id=*/1,
      StandaloneFetch{
          FullTrackName("foo", "bar"),
          /*start_location=*/Location{1, 2},
          /*end_location=*/Location{5, 6},
      },
      MessageParameters(),  // set in constructor.
  };
};

// This is not used in the parameterized Parser and Framer tests, because it
// does not have its own MoqtMessageType.
class QUICHE_NO_EXPORT RelativeJoiningFetchMessage : public TestMessageBase {
 public:
  RelativeJoiningFetchMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
    fetch_.parameters.authorization_tokens.push_back(
        AuthToken(AuthTokenType::kOutOfBand, "baz"));
    fetch_.parameters.group_order = MoqtGroupOrder::kAscending;
    fetch_.parameters.subscriber_priority = 2;
  }
  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtFetch>(values);
    if (cast.request_id != fetch_.request_id) {
      QUIC_LOG(INFO) << "FETCH request_id mismatch";
      return false;
    }
    if (cast.fetch != fetch_.fetch) {
      QUIC_LOG(INFO) << "FETCH mismatch";
      return false;
    }
    if (cast.parameters != fetch_.parameters) {
      QUIC_LOG(INFO) << "FETCH parameters mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vvvvvvv-----v-v-"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(fetch_);
  }

  void SetGroupOrder(uint8_t group_order) {
    raw_packet_[18] = static_cast<uint8_t>(group_order);
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

 private:
  uint8_t raw_packet_[19] = {
      0x16, 0x00, 0x10,
      0x01,        // request_id = 1
      0x02,        // type = kRelativeJoining
      0x02, 0x02,  // joining_request_id = 2, 2 groups
      0x03,        // 3 parameters
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x7a,  // token = "baz"
      0x1d, 0x02,                                // priority = kHigh
      0x02, 0x01,                                // group_order = kAscending
  };

  MoqtFetch fetch_ = {
      /*request_id =*/1,
      JoiningFetchRelative{2, 2},
      MessageParameters(),  // set in constructor.
  };
};

// This is not used in the parameterized Parser and Framer tests, because it
// does not have its own MoqtMessageType.
class QUICHE_NO_EXPORT AbsoluteJoiningFetchMessage : public TestMessageBase {
 public:
  AbsoluteJoiningFetchMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
    fetch_.parameters.authorization_tokens.push_back(
        AuthToken(AuthTokenType::kOutOfBand, "baz"));
    fetch_.parameters.group_order = MoqtGroupOrder::kAscending;
    fetch_.parameters.subscriber_priority = 2;
  }
  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtFetch>(values);
    if (cast.request_id != fetch_.request_id) {
      QUIC_LOG(INFO) << "FETCH request_id mismatch";
      return false;
    }
    if (cast.fetch != fetch_.fetch) {
      QUIC_LOG(INFO) << "FETCH mismatch";
      return false;
    }
    if (cast.parameters != fetch_.parameters) {
      QUIC_LOG(INFO) << "FETCH parameters mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vvvvvvv-----v-v-"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(fetch_);
  }

  void SetGroupOrder(uint8_t group_order) {
    raw_packet_[5] = static_cast<uint8_t>(group_order);
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

 private:
  uint8_t raw_packet_[19] = {
      0x16, 0x00, 0x10,
      0x01,        // request_id = 1
      0x03,        // type = kAbsoluteJoining
      0x02, 0x02,  // joining_request_id = 2, group_id = 2
      0x03,        // 3 parameters
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x7a,  // token = "baz"
      0x1d, 0x02,                                // priority = kHigh
      0x02, 0x01,                                // group_order = kAscending
  };

  MoqtFetch fetch_ = {
      /*request_id=*/1,
      JoiningFetchAbsolute{2, 2},
      MessageParameters(),  // set in constructor.
  };
};

class QUICHE_NO_EXPORT FetchOkMessage : public TestMessageBase {
 public:
  FetchOkMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }
  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtFetchOk>(values);
    if (cast.end_of_track != fetch_ok_.end_of_track) {
      QUIC_LOG(INFO) << "FETCH_OK end_of_track mismatch";
      return false;
    }
    if (cast.end_location != fetch_ok_.end_location) {
      QUIC_LOG(INFO) << "FETCH_OK end_location mismatch";
      return false;
    }
    if (cast.parameters != fetch_ok_.parameters) {
      QUIC_LOG(INFO) << "FETCH_OK parameters mismatch";
      return false;
    }
    if (cast.properties != fetch_ok_.properties) {
      QUIC_LOG(INFO) << "FETCH_OK properties mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("-vvvv--vv"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(fetch_ok_);
  }

 private:
  uint8_t raw_packet_[12] = {
      0x18, 0x00, 0x09,
      0x00,              // end_of_track = false
      0x05, 0x04,        // end_location = 5, 3
      0x00,              // no parameters
      0x04, 0xa7, 0x10,  // MaxCacheDuration = 10000
      0x1e, 0x02,        // group_order = kDescending
  };

  MoqtFetchOk fetch_ok_ = {
      /*end_of_track=*/false,
      /*end_location=*/Location{5, 3},
      MessageParameters(),
      TrackProperties(std::nullopt,
                      quic::QuicTimeDelta::FromMilliseconds(10000),
                      std::nullopt, std::nullopt, MoqtGroupOrder::kDescending,
                      std::nullopt, std::nullopt),
  };
};

class QUICHE_NO_EXPORT PublishMessage : public TestMessageBase {
 public:
  PublishMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
    publish_.parameters.authorization_tokens.push_back(
        AuthToken(AuthTokenType::kOutOfBand, "baz"));
    publish_.parameters.largest_object = Location(10, 1);
    publish_.parameters.set_forward(true);
  }
  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtPublish>(values);
    if (cast.request_id != publish_.request_id) {
      QUIC_LOG(INFO) << "PUBLISH request_id mismatch";
      return false;
    }
    if (cast.full_track_name != publish_.full_track_name) {
      QUIC_LOG(INFO) << "PUBLISH full_track_name mismatch";
      return false;
    }
    if (cast.track_alias != publish_.track_alias) {
      QUIC_LOG(INFO) << "PUBLISH track_alias mismatch";
      return false;
    }
    if (cast.parameters != publish_.parameters) {
      QUIC_LOG(INFO) << "PUBLISH parameters mismatch";
      return false;
    }
    if (cast.properties != publish_.properties) {
      QUIC_LOG(INFO) << "PUBLISH properties mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override {
    ExpandVarintsImpl("vvv---v---vvvv-----vvvv-v-");
  }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(publish_);
  }

 private:
  uint8_t raw_packet_[29] = {
      0x1d, 0x00, 0x1a,
      0x01,                                      // request_id = 1
      0x01, 0x03, 0x66, 0x6f, 0x6f,              // track_namespace = "foo"
      0x03, 0x62, 0x61, 0x72,                    // track_name = "bar"
      0x04,                                      // track_alias = 4
      0x03,                                      // 3 parameters
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x7a,  // token = "baz"
      0x06, 0x0a, 0x01,                          // largest_object = 10, 1
      0x07, 0x01,                                // forward = 1
      0x22, 0x02,                                // group_order = kAscending
  };

  MoqtPublish publish_ = {
      /*request_id=*/1,
      FullTrackName("foo", "bar"),
      /*track_alias=*/4,
      MessageParameters(),
      TrackProperties(std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                      MoqtGroupOrder::kDescending, std::nullopt, std::nullopt),
  };
};

class QUICHE_NO_EXPORT PublishStateNotifyMessage : public TestMessageBase {
 public:
  PublishStateNotifyMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
    publish_state_notify_.parameters.largest_object = Location(10, 1);
    publish_state_notify_.parameters.set_forward(true);
    publish_state_notify_.parameters.location_filter =
        LocationFilter::AbsoluteStartGroupEndFilter(Location(3, 1), 5);
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtPublishStateNotify>(values);
    if (cast.parameters != publish_state_notify_.parameters) {
      QUIC_LOG(INFO) << "PUBLISH_STATE_NOTIFY parameter mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vvvvv-vvvvv"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(publish_state_notify_);
  }

 private:
  uint8_t raw_packet_[14] = {
      0x22, 0x00, 0x0b,
      0x03,                          // 3 parameters
      0x09, 0x0a, 0x01,              // largest_object = (10, 1)
      0x07, 0x01,                    // forward = true
      0x11, 0x03, 0x03, 0x01, 0x02,  // AbsoluteStartGroupEnd: (3, 1) to 5
  };

  MoqtPublishStateNotify publish_state_notify_ = {
      MessageParameters(),  // Set in the constructor.
  };
};

class QUICHE_NO_EXPORT ObjectAckMessage : public TestMessageBase {
 public:
  ObjectAckMessage() : TestMessageBase() {
    SetWireImage(raw_packet_, sizeof(raw_packet_));
  }

  bool EqualFieldValues(const MessageStructuredData& values) const override {
    auto cast = std::get<MoqtObjectAck>(values);
    if (cast.group_id != object_ack_.group_id) {
      QUIC_LOG(INFO) << "OBJECT_ACK group ID mismatch";
      return false;
    }
    if (cast.object_id != object_ack_.object_id) {
      QUIC_LOG(INFO) << "OBJECT_ACK object ID mismatch";
      return false;
    }
    if (cast.delta_from_deadline != object_ack_.delta_from_deadline) {
      QUIC_LOG(INFO) << "OBJECT_ACK delta from deadline mismatch";
      return false;
    }
    return true;
  }

  void ExpandVarints() override { ExpandVarintsImpl("vvv"); }

  MessageStructuredData structured_data() const override {
    return TestMessageBase::MessageStructuredData(object_ack_);
  }

 private:
  uint8_t raw_packet_[7] = {
      0xb1, 0x84, 0x00, 0x03,  // type
      0x10, 0x20,              // group, object
      0x20,                    // 0x10 time delta
  };

  MoqtObjectAck object_ack_ = {
      /*group_id=*/0x10,
      /*object_id=*/0x20,
      /*delta_from_deadline=*/quic::QuicTimeDelta::FromMicroseconds(0x10),
  };
};

// Factory function for test messages.
static inline std::unique_ptr<TestMessageBase> CreateTestMessage(
    MoqtMessageType message_type, bool is_webtrans = true,
    quic::Perspective perspective = quic::Perspective::IS_CLIENT) {
  switch (message_type) {
    case MoqtMessageType::kRequestOk:
      return std::make_unique<RequestOkMessage>();
    case MoqtMessageType::kRequestError:
      return std::make_unique<RequestErrorMessage>();
    case MoqtMessageType::kSubscribe:
      return std::make_unique<SubscribeMessage>();
    case MoqtMessageType::kSubscribeOk:
      return std::make_unique<SubscribeOkMessage>();
    case MoqtMessageType::kPublishDone:
      return std::make_unique<PublishDoneMessage>();
    case MoqtMessageType::kRequestUpdate:
      return std::make_unique<RequestUpdateMessage>();
    case MoqtMessageType::kPublishNamespace:
      return std::make_unique<PublishNamespaceMessage>();
    case MoqtMessageType::kNamespace:
      return std::make_unique<NamespaceMessage>();
    case MoqtMessageType::kNamespaceDone:
      return std::make_unique<NamespaceDoneMessage>();
    case MoqtMessageType::kPublishSkipped:
      return std::make_unique<PublishSkippedMessage>();
    case MoqtMessageType::kTrackStatus:
      return std::make_unique<TrackStatusMessage>();
    case MoqtMessageType::kGoAway:
      return std::make_unique<GoAwayMessage>();
    case MoqtMessageType::kSubscribeNamespace:
      return std::make_unique<SubscribeNamespaceMessage>();
    case MoqtMessageType::kSubscribeTracks:
      return std::make_unique<SubscribeTracksMessage>();
    case MoqtMessageType::kFetch:
      return std::make_unique<FetchMessage>();
    case MoqtMessageType::kFetchOk:
      return std::make_unique<FetchOkMessage>();
    case MoqtMessageType::kPublish:
      return std::make_unique<PublishMessage>();
    case MoqtMessageType::kPublishStateNotify:
      return std::make_unique<PublishStateNotifyMessage>();
    case MoqtMessageType::kObjectAck:
      return std::make_unique<ObjectAckMessage>();
    case MoqtMessageType::kSetup:
      if (perspective == quic::Perspective::IS_CLIENT) {
        return std::make_unique<ClientSetupMessage>(is_webtrans);
      } else {
        return std::make_unique<ServerSetupMessage>();
      }
    default:
      return nullptr;
  }
}

static inline std::unique_ptr<TestMessageBase> CreateTestDataStream(
    MoqtDataStreamType type) {
  if (type.IsFetch()) {
    return std::make_unique<StreamHeaderFetchMessage>();
  }
  return std::make_unique<StreamHeaderSubgroupMessage>(type);
}

static inline std::unique_ptr<TestMessageBase> CreateTestFetch(
    MoqtFetchSerialization type) {
  return std::make_unique<StreamMiddlerFetchMessage>(type);
}

static inline std::unique_ptr<TestMessageBase> CreateTestDatagram(
    MoqtDatagramType type) {
  return std::make_unique<ObjectDatagramMessage>(type);
}

}  // namespace moqt::test

#endif  // QUICHE_QUIC_MOQT_TEST_TOOLS_MOQT_TEST_MESSAGE_H_
