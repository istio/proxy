// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_parser.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/casts.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/fixed_array.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/http2/adapter/header_validator.h"
#include "quiche/quic/core/quic_data_reader.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/common/moq_varint.h"
#include "quiche/common/platform/api/quiche_bug_tracker.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/quiche_checked_math.h"
#include "quiche/common/quiche_data_reader.h"
#include "quiche/common/quiche_endian.h"
#include "quiche/common/quiche_status_utils.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt {

namespace {

uint64_t SignedVarintUnserializedForm(uint64_t value) {
  if (value & 0x01) {
    return -(value >> 1);
  }
  return value >> 1;
}

absl::Status KeyValueFormatError(absl::string_view message) {
  return MoqtErrorStatusWithCode(message, MoqtError::kKeyValueFormattingError);
}

absl::Status CheckForTrailingData(const quic::QuicDataReader& reader) {
  if (!reader.IsDoneReading()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Control message has excess data of ",
                     reader.BytesRemaining(), " bytes at the end"));
  }
  return absl::OkStatus();
}

absl::Status SafeIncrementByWithStatus(uint64_t& a, uint64_t b) {
  return quiche::SafeIncrementBy(a, b)
             ? absl::OkStatus()
             : absl::InvalidArgumentError("Integer overflow encountered");
}

// |fin_read| is set to true if there is a FIN anywhere before the end of the
// varint.
std::optional<uint64_t> ReadMoqVarIntFromStream(webtransport::Stream& stream,
                                                bool& fin_read) {
  fin_read = false;

  webtransport::Stream::PeekResult peek_result =
      stream.PeekNextReadableRegion();
  if (peek_result.peeked_data.empty()) {
    if (peek_result.fin_next) {
      fin_read = stream.SkipBytes(0);
      QUICHE_DCHECK(fin_read);
    }
    return std::nullopt;
  }
  char first_byte = peek_result.peeked_data[0];
  size_t varint_size = quiche::GetMoqVarintLengthForFirstByte(
      absl::bit_cast<uint8_t>(first_byte));
  if (stream.ReadableBytes() < varint_size) {
    if (peek_result.all_data_received) {
      fin_read = true;
    }
    return std::nullopt;
  }

  char buffer[9];
  absl::Span<char> bytes_to_read =
      absl::MakeSpan(buffer).subspan(0, varint_size);
  webtransport::Stream::ReadResult read_result = stream.Read(bytes_to_read);
  QUICHE_DCHECK_EQ(read_result.bytes_read, varint_size);
  fin_read = read_result.fin;

  quiche::QuicheDataReader reader(buffer, read_result.bytes_read);
  uint64_t result;
  bool success = reader.ReadMoqVarInt(&result);
  QUICHE_DCHECK(success);
  QUICHE_DCHECK(reader.IsDoneReading());
  return result;
}

bool ParseAuthToken(absl::string_view field, std::vector<AuthToken>& out) {
  quic::QuicDataReader reader(field);
  AuthTokenAliasType alias_type;
  uint64_t alias;
  AuthTokenType type;
  absl::string_view token;
  uint64_t value;
  if (!reader.ReadMoqVarInt(&value)) {
    return false;
  }
  alias_type = static_cast<AuthTokenAliasType>(value);
  switch (alias_type) {
    case AuthTokenAliasType::kUseValue:
      if (!reader.ReadMoqVarInt(&value) ||
          value > AuthTokenType::kMaxAuthTokenType) {
        return false;
      }
      type = static_cast<AuthTokenType>(value);
      token = reader.PeekRemainingPayload();
      out.push_back(AuthToken(type, token));
      break;
    case AuthTokenAliasType::kUseAlias:
      if (!reader.ReadMoqVarInt(&value)) {
        return false;
      }
      out.push_back(AuthToken(value, alias_type));
      break;
    case AuthTokenAliasType::kRegister:
      if (!reader.ReadMoqVarInt(&alias) || !reader.ReadMoqVarInt(&value)) {
        return false;
      }
      type = static_cast<AuthTokenType>(value);
      token = reader.PeekRemainingPayload();
      out.push_back(AuthToken(alias, type, token));
      break;
    case AuthTokenAliasType::kDelete:
      if (!reader.ReadMoqVarInt(&alias)) {
        return false;
      }
      out.push_back(AuthToken(alias, alias_type));
      break;
    default:  // invalid alias type
      return false;
  }
  return true;
}

absl::Status ParseLocationFilter(quic::QuicDataReader& reader,
                                 std::optional<LocationFilter>& out) {
  uint64_t type;
  if (!reader.ReadMoqVarInt(&type)) {
    return absl::InvalidArgumentError("Unable to read location filter type");
  }
  uint64_t group, object, end_group, end_object;
  switch (static_cast<MoqtLocationFilterType>(type)) {
    case MoqtLocationFilterType::kNoFilter:
      out = LocationFilter::NoLocationFilter();
      break;
    case MoqtLocationFilterType::kNextObject:
      out = LocationFilter::NextObjectFilter();
      break;
    case MoqtLocationFilterType::kRelativeStart:
      if (!reader.ReadMoqVarInt(&group)) {
        return absl::InvalidArgumentError("Invalid RelativeStart filter");
      }
      out = LocationFilter::RelativeStartFilter(group);
      break;
    case MoqtLocationFilterType::kAbsoluteStart:
      if (!reader.ReadMoqVarInt(&group) || !reader.ReadMoqVarInt(&object)) {
        return absl::InvalidArgumentError("Invalid AbsoluteStart filter");
      }
      out = LocationFilter::AbsoluteStartFilter(Location(group, object));
      break;
    case MoqtLocationFilterType::kAbsoluteStartGroupEnd:
      if (!reader.ReadMoqVarInt(&group) || !reader.ReadMoqVarInt(&object) ||
          !reader.ReadMoqVarInt(&end_group)) {
        return absl::InvalidArgumentError(
            "Invalid AbsoluteStartGroupEnd filter");
      }
      QUICHE_RETURN_IF_ERROR(SafeIncrementByWithStatus(end_group, group));
      out = LocationFilter::AbsoluteStartGroupEndFilter(Location(group, object),
                                                        end_group);
      break;
    case MoqtLocationFilterType::kAbsoluteRange:
      if (!reader.ReadMoqVarInt(&group) || !reader.ReadMoqVarInt(&object) ||
          !reader.ReadMoqVarInt(&end_group) ||
          !reader.ReadMoqVarInt(&end_object)) {
        return absl::InvalidArgumentError("Invalid AbsoluteRange filter");
      }
      QUICHE_RETURN_IF_ERROR(SafeIncrementByWithStatus(end_group, group));
      if (Location(end_group, end_object) < Location(group, object)) {
        return absl::InvalidArgumentError(
            "End object comes before start object");
      }
      out = LocationFilter::AbsoluteRangeFilter(
          Location(group, object), Location(end_group, end_object));
      break;
    default:  // invalid filter type
      return absl::InvalidArgumentError("Invalid filter type");
  }
  return absl::OkStatus();
}

absl::Status ReadTrackNamespace(quic::QuicDataReader& reader,
                                TrackNamespace& track_namespace) {
  QUICHE_DCHECK(track_namespace.empty());
  uint64_t num_elements;
  if (!reader.ReadMoqVarInt(&num_elements)) {
    return absl::InvalidArgumentError(
        "Unable to parse the number of namespace elements");
  }
  if (num_elements > kMaxNamespaceElements) {
    return absl::InvalidArgumentError("Invalid number of namespace elements");
  }
  if (num_elements == 0) {
    return absl::OkStatus();
  }
  absl::FixedArray<absl::string_view> elements(num_elements);
  for (uint64_t i = 0; i < num_elements; ++i) {
    if (!reader.ReadStringPieceMoqVarInt(&elements[i])) {
      return absl::InvalidArgumentError(
          "Namespace element shorter than specified");
    }
  }
  if (!track_namespace.Append(elements)) {
    return absl::InvalidArgumentError("Track namespace is too large");
  }
  return absl::OkStatus();
}

absl::Status ReadFullTrackName(quic::QuicDataReader& reader,
                               FullTrackName& full_track_name) {
  QUICHE_DCHECK(!full_track_name.IsValid());
  TrackNamespace track_namespace;
  QUICHE_RETURN_IF_ERROR(ReadTrackNamespace(reader, track_namespace));
  absl::string_view name;
  if (!reader.ReadStringPieceMoqVarInt(&name)) {
    return absl::InvalidArgumentError("Unable to parse track name");
  }
  absl::StatusOr<FullTrackName> full_track_name_or =
      FullTrackName::Create(std::move(track_namespace), std::string(name));
  QUICHE_RETURN_IF_ERROR(full_track_name_or.status());
  full_track_name = *std::move(full_track_name_or);
  return absl::OkStatus();
}

absl::StatusOr<MessageParameters> ParseMessageParameters(
    quic::QuicDataReader& reader) {
  MessageParameters params;
  uint64_t num_params;
  if (!reader.ReadMoqVarInt(&num_params)) {
    return absl::InvalidArgumentError(
        "Unable to parse key-value pair list element count");
  }
  uint64_t type = 0;
  uint8_t value8;
  uint64_t value64;
  absl::string_view value_str;
  for (uint64_t i = 0; i < num_params; ++i) {
    uint64_t type_diff;
    if (!reader.ReadMoqVarInt(&type_diff)) {
      return absl::InvalidArgumentError("Unable to parse the parameter type");
    }
    QUICHE_RETURN_IF_ERROR(SafeIncrementByWithStatus(type, type_diff));
    switch (static_cast<MessageParameter>(type)) {
      case MessageParameter::kObjectDeliveryTimeout:
        if (params.object_delivery_timeout.has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        if (!reader.ReadMoqVarInt(&value64)) {
          return absl::InvalidArgumentError("Parameter parsing error");
        }
        if (value64 == 0) {
          params.object_delivery_timeout = quic::QuicTimeDelta::Infinite();
          break;
        }
        params.object_delivery_timeout =
            quic::QuicTimeDelta::TryFromMilliseconds(value64).value_or(
                quic::QuicTimeDelta::Infinite());
        break;
      case MessageParameter::kAuthorizationToken:
        if (!reader.ReadStringPieceMoqVarInt(&value_str)) {
          return absl::InvalidArgumentError("Parameter parsing error");
        }
        if (!ParseAuthToken(value_str, params.authorization_tokens)) {
          return absl::InvalidArgumentError("Malformed auth token parameter");
        }
        break;
      case MessageParameter::kRendezvousTimeout:
        if (params.rendezvous_timeout.has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        if (!reader.ReadMoqVarInt(&value64)) {
          return absl::InvalidArgumentError("Parameter parsing error");
        }
        params.rendezvous_timeout =
            std::min(kMaxRendezvousTimeout,
                     quic::QuicTimeDelta::TryFromMilliseconds(value64).value_or(
                         quic::QuicTimeDelta::Infinite()));
        break;
      case MessageParameter::kSubgroupDeliveryTimeout:
        if (params.subgroup_delivery_timeout.has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        if (!reader.ReadMoqVarInt(&value64)) {
          return absl::InvalidArgumentError("Parameter parsing error");
        }
        if (value64 == 0) {
          params.subgroup_delivery_timeout = quic::QuicTimeDelta::Infinite();
          break;
        }
        params.subgroup_delivery_timeout =
            quic::QuicTimeDelta::TryFromMilliseconds(value64).value_or(
                quic::QuicTimeDelta::Infinite());
        break;
      case MessageParameter::kExpires:
        if (params.expires.has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        if (!reader.ReadMoqVarInt(&value64)) {
          return absl::InvalidArgumentError("Parameter parsing error");
        }
        params.expires =
            quic::QuicTimeDelta::TryFromMilliseconds(value64).value_or(
                quic::QuicTimeDelta::Infinite());
        if (params.expires->IsZero()) {
          params.expires = quic::QuicTimeDelta::Infinite();
        }
        break;
      case MessageParameter::kLargestObject: {
        uint64_t group, object;
        if (params.largest_object.has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        if (!reader.ReadMoqVarInt(&group) || !reader.ReadMoqVarInt(&object)) {
          return absl::InvalidArgumentError("Parameter parsing error");
        }
        params.largest_object = Location(group, object);
        break;
      }
      case MessageParameter::kFillTimeout:
        if (params.fill_timeout.has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        if (!reader.ReadMoqVarInt(&value64)) {
          return absl::InvalidArgumentError("Parameter parsing error");
        }
        params.fill_timeout =
            quic::QuicTimeDelta::TryFromMilliseconds(value64).value_or(
                quic::QuicTimeDelta::Infinite());
        break;
      case MessageParameter::kForward:
        if (params.forward_has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        if (!reader.ReadUInt8(&value8)) {
          return absl::InvalidArgumentError("Parameter parsing error");
        }
        if (value8 > 1) {
          return absl::InvalidArgumentError("FORWARD must be 0 or 1");
        }
        params.set_forward(value8 != 0);
        break;
      case MessageParameter::kSubscriberPriority:
        if (params.subscriber_priority.has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        if (!reader.ReadUInt8(&value8)) {
          return absl::InvalidArgumentError("Parameter parsing error");
        }
        params.subscriber_priority = static_cast<MoqtPriority>(value8);
        break;
      case MessageParameter::kLocationFilter:
        if (params.location_filter.has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        QUICHE_RETURN_IF_ERROR(
            ParseLocationFilter(reader, params.location_filter));
        break;
      case MessageParameter::kGroupOrder:
        if (params.group_order.has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        if (!reader.ReadUInt8(&value8)) {
          return absl::InvalidArgumentError("Parameter parsing error");
        }
        if (value8 > kMaxMoqtGroupOrder || value8 < kMinMoqtGroupOrder) {
          return absl::InvalidArgumentError(
              "GROUP_ORDER is outside the valid range");
        }
        params.group_order = static_cast<MoqtGroupOrder>(value8);
        break;
      case MessageParameter::kNewGroupRequest:
        if (params.new_group_request.has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        if (!reader.ReadMoqVarInt(&value64)) {
          return absl::InvalidArgumentError("Parameter parsing error");
        }
        params.new_group_request = value64;
        break;
      case MessageParameter::kTrackNamespacePrefix:
        if (params.track_namespace_prefix.has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        params.track_namespace_prefix.emplace();
        QUICHE_RETURN_IF_ERROR(
            ReadTrackNamespace(reader, *params.track_namespace_prefix));
        break;
      case MessageParameter::kOackWindowSize:
        if (params.oack_window_size.has_value()) {
          return absl::InvalidArgumentError("Duplicate Message Parameter");
        }
        if (!reader.ReadMoqVarInt(&value64)) {
          return absl::InvalidArgumentError("Parameter parsing error");
        }
        // Since QuicTimeDelta stores an int64_t value in microseconds, it is
        // possible for the value to be out of range.
        params.oack_window_size =
            value64 > std::numeric_limits<int64_t>::max()
                ? quic::QuicTimeDelta::Infinite()
                : quic::QuicTimeDelta::FromMicroseconds(value64);
        break;
      default:
        // Unknown MessageParameters not allowed!
        return absl::InvalidArgumentError(
            absl::StrCat("Unknown message parameter 0x", absl::Hex(type)));
    }
  }
  return params;
}

absl::Status ParseKeyValuePairList(quic::QuicDataReader& reader,
                                   KeyValuePairList& list) {
  list.clear();
  uint64_t type = 0;
  while (reader.BytesRemaining() > 0) {
    uint64_t type_diff;
    if (!reader.ReadMoqVarInt(&type_diff)) {
      return absl::InvalidArgumentError(
          "Unable to parse the key in a key-value pair");
    }
    QUICHE_RETURN_IF_ERROR(SafeIncrementByWithStatus(type, type_diff));
    if (type % 2 == 1) {
      absl::string_view bytes;
      if (!reader.ReadStringPieceMoqVarInt(&bytes)) {
        return absl::InvalidArgumentError(
            "Unable to read the string value in a key-value pair");
      }
      list.insert(type, bytes);
      continue;
    }
    uint64_t value;
    if (!reader.ReadMoqVarInt(&value)) {
      return absl::InvalidArgumentError(
          "Unable to read the integer value in a key-value pair");
    }
    list.insert(type, value);
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status SetupOptions::FromKeyValuePairList(const KeyValuePairList& list) {
  absl::Status status = absl::OkStatus();
  uint64_t last_key;
  bool result = list.ForEach(
      [&](uint64_t key, std::variant<uint64_t, absl::string_view> value) {
        last_key = key;
        switch (static_cast<SetupOption>(key)) {
          case SetupOption::kMaxAuthTokenCacheSize:
            if (max_auth_token_cache_size.has_value()) {
              status = absl::InvalidArgumentError("Duplicate Setup Option");
              return false;
            }
            max_auth_token_cache_size = std::get<uint64_t>(value);
            break;
          case SetupOption::kPath:
            if (path.has_value()) {
              status = absl::InvalidArgumentError("Duplicate Setup Option");
              return false;
            }
            if (!http2::adapter::HeaderValidator::IsValidPath(
                    std::get<absl::string_view>(value),
                    /*allow_fragment=*/false)) {
              status = MoqtErrorStatusWithCode("Malformed path",
                                               MoqtError::kMalformedPath);
              return false;
            }
            path = std::get<absl::string_view>(value);
            break;
          case SetupOption::kAuthorizationToken:
            if (!ParseAuthToken(std::get<absl::string_view>(value),
                                authorization_tokens)) {
              status = KeyValueFormatError("Malformed auth token option");
              return false;
            }
            break;
          case SetupOption::kAuthority:
            if (!http2::adapter::HeaderValidator::IsValidAuthority(
                    std::get<absl::string_view>(value))) {
              status = MoqtErrorStatusWithCode("Invalid authority field",
                                               MoqtError::kMalformedAuthority);
              return false;
            }
            authority = std::get<absl::string_view>(value);
            break;
          case SetupOption::kMoqtImplementation:
            if (moqt_implementation.has_value()) {
              status = absl::InvalidArgumentError("Duplicate Setup Option");
              return false;
            }
            QUICHE_LOG(INFO) << "Peer MOQT implementation: "
                             << std::get<absl::string_view>(value);
            moqt_implementation = std::get<absl::string_view>(value);
            break;
          case SetupOption::kSupportObjectAcks:
            if (support_object_acks.has_value()) {
              status = absl::InvalidArgumentError("Duplicate Setup Option");
              return false;
            }
            if (std::get<uint64_t>(value) > 1) {
              status =
                  KeyValueFormatError("SUPPORT_OBJECT_ACKS has to be 0 or 1");
              return false;
            }
            support_object_acks = (std::get<uint64_t>(value) == 1);
            break;
          default:
            break;
        }
        return true;
      });
  if (!result && status.ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse the value for the Setup Option key 0x",
                     absl::Hex(static_cast<uint64_t>(last_key))));
  }
  return status;
}

MoqtStreamTypeParser::MoqtStreamTypeParser(
    MoqtStreamTypeParser&& other) noexcept
    : stream_(other.stream_), type_(other.type_), status_(other.status_) {
  other.status_ = absl::InternalError("Accessing a moved-from parser");
}

MoqtStreamTypeParser& MoqtStreamTypeParser::operator=(
    MoqtStreamTypeParser&& other) noexcept {
  if (this != &other) {
    stream_ = other.stream_;
    type_ = other.type_;
    status_ = other.status_;
    other.status_ = absl::InternalError("Accessing a moved-from parser");
  }
  return *this;
}

absl::StatusOr<uint64_t> MoqtStreamTypeParser::ReadStreamType() {
  if (!status_.ok()) {
    return status_;
  }
  if (type_.has_value()) {
    // The type has already been read; the rest of the stream should be only
    // read by the next parser.
    return *type_;
  }
  bool fin_read = false;
  std::optional<uint64_t> type = ReadMoqVarIntFromStream(*stream_, fin_read);
  if (fin_read && type != kPaddingStreamType) {
    // Besides padding streams, all other streams require some data after the
    // type byte.
    status_ = absl::InvalidArgumentError(
        "FIN received before or immediately after the stream type");
    return status_;
  }
  if (!type.has_value()) {
    return absl::UnavailableError("No complete message available");
  }
  type_ = *type;
  return *type_;
}

MoqtControlStreamParser::MoqtControlStreamParser(
    MoqtStreamTypeParser type_parser)
    : stream_(*type_parser.stream()),
      current_message_type_(type_parser.stream_type()),
      fin_read_(false) {}

absl::StatusOr<MoqtRawControlMessage>
MoqtControlStreamParser::ReadNextMessage() {
  if (error_encountered_ || fin_read_) {
    return absl::FailedPreconditionError(
        "Trying to read from a control stream after an error or an EOF "
        "occurred.");
  }
  absl::StatusOr<MoqtRawControlMessage> result = ReadNextMessageInner();
  if (!result.ok() && !absl::IsUnavailable(result.status())) {
    error_encountered_ = true;
  } else {
    if (fin_read_ && !allow_fin_) {
      result = absl::InvalidArgumentError(
          "Unexpected FIN on a control stream (no FINs are allowed on this "
          "stream)");
      error_encountered_ = true;
    }
  }
  return result;
}

absl::Status MoqtControlStreamParser::ReadMessageType() {
  if (current_message_type_.has_value()) {
    QUICHE_BUG(MoqtControlStreamParser_ReadMessageType_bad_state)
        << "ReadMessageType() called in an invalid state";
    return absl::InternalError("ReadMessageType() called in an invalid state");
  }
  current_message_type_ = ReadMoqVarIntFromStream(stream_, fin_read_);
  if (!current_message_type_.has_value()) {
    webtransport::Stream::PeekResult peek_result =
        stream_.PeekNextReadableRegion();
    if (peek_result.all_data_received && !peek_result.peeked_data.empty()) {
      return absl::InvalidArgumentError(
          "Unexpected FIN on a control stream (FIN received in the middle of "
          "type)");
    }
    return absl::UnavailableError("No complete message available");
  }
  if (fin_read_) {
    return absl::InvalidArgumentError(
        "Unexpected FIN on a control stream (FIN received immediately after "
        "type)");
  }
  return absl::OkStatus();
}

absl::StatusOr<MoqtRawControlMessage>
MoqtControlStreamParser::ReadNextMessageInner() {
  if (!current_message_type_.has_value()) {
    QUICHE_RETURN_IF_ERROR(ReadMessageType());
  }

  if (!current_message_remaining_.has_value()) {
    uint16_t message_size = 0;
    std::array<char, sizeof(message_size)> buffer;
    if (stream_.ReadableBytes() < buffer.size()) {
      if (stream_.PeekNextReadableRegion().all_data_received) {
        return absl::InvalidArgumentError(
            "Unexpected FIN on a control stream (FIN received in the middle of "
            "the message size)");
      }
      return absl::UnavailableError("No complete message available");
    }
    webtransport::Stream::ReadResult read_result =
        stream_.Read(absl::MakeSpan(buffer));
    fin_read_ |= read_result.fin;
    QUICHE_DCHECK_EQ(read_result.bytes_read, buffer.size());

    memcpy(&message_size, buffer.data(), buffer.size());
    message_size = quiche::QuicheEndian::NetToHost16(message_size);
    if (message_size > kMaxMessageHeaderSize) {
      return absl::InvalidArgumentError(
          absl::StrCat("A control message exceeds the maximum allowed size of ",
                       kMaxMessageHeaderSize, " bytes"));
    }
    current_message_.resize(message_size);
    current_message_remaining_ = absl::MakeSpan(current_message_);
  }

  QUICHE_DCHECK(current_message_remaining_.has_value());
  if (!current_message_remaining_->empty()) {
    webtransport::Stream::ReadResult read_result =
        stream_.Read(*current_message_remaining_);
    current_message_remaining_->remove_prefix(read_result.bytes_read);
    fin_read_ |= read_result.fin;
  }
  if (!current_message_remaining_->empty()) {
    if (fin_read_) {
      return absl::InvalidArgumentError(absl::StrCat(
          "FIN encountered when there are ", current_message_remaining_->size(),
          " bytes left in the current message"));
    }
    return absl::UnavailableError("No complete message available");
  }
  MoqtRawControlMessage message{
      .type = static_cast<MoqtMessageType>(*current_message_type_),
      .payload = std::move(current_message_)};
  current_message_type_.reset();
  current_message_remaining_.reset();
  // Technically, std::move() leaves `current_message_` in a
  // "valid but undefined state"; clear it out explicitly.
  current_message_.clear();
  return message;
}

absl::StatusOr<MoqtSetup> MoqtControlMessageParser::ProcessSetup(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtSetup setup;
  KeyValuePairList options;
  QUICHE_RETURN_IF_ERROR(ParseKeyValuePairList(reader, options));
  QUICHE_RETURN_IF_ERROR(FillAndValidateSetupOptions(options, setup.options));
  // TODO(martinduke): Validate construction of the PATH (Sec 8.3.2.1)
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return setup;
}

absl::StatusOr<MoqtSubscribe> MoqtControlMessageParser::ProcessSubscribe(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtSubscribe subscribe;
  if (!reader.ReadMoqVarInt(&subscribe.request_id)) {
    return absl::InvalidArgumentError("Failed to read request ID");
  }
  QUICHE_RETURN_IF_ERROR(ReadFullTrackName(reader, subscribe.full_track_name));
  QUICHE_ASSIGN_OR_RETURN(subscribe.parameters, ParseMessageParameters(reader));
  if (!ParametersAllowedByMessage(subscribe.parameters,
                                  MoqtMessageType::kSubscribe)) {
    return absl::InvalidArgumentError("Invalid SUBSCRIBE message parameters");
  }
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return subscribe;
}

absl::StatusOr<MoqtSubscribeOk> MoqtControlMessageParser::ProcessSubscribeOk(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtSubscribeOk subscribe_ok;
  if (!reader.ReadMoqVarInt(&subscribe_ok.track_alias)) {
    return absl::InvalidArgumentError("Failed to read the track alias");
  }
  QUICHE_ASSIGN_OR_RETURN(subscribe_ok.parameters,
                          ParseMessageParameters(reader));
  if (!ParametersAllowedByMessage(subscribe_ok.parameters,
                                  MoqtMessageType::kSubscribeOk)) {
    return absl::InvalidArgumentError(
        "Invalid SUBSCRIBE_OK message parameters");
  }
  QUICHE_RETURN_IF_ERROR(
      ParseKeyValuePairList(reader, subscribe_ok.properties));
  if (!subscribe_ok.properties.Validate()) {
    return absl::InvalidArgumentError("Invalid SUBSCRIBE_OK track properties");
  }
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return subscribe_ok;
}

absl::StatusOr<MoqtRequestError> MoqtControlMessageParser::ProcessRequestError(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtRequestError request_error;
  uint64_t error_code;
  uint64_t raw_interval;
  if (!reader.ReadMoqVarInt(&error_code) ||
      !reader.ReadMoqVarInt(&raw_interval) ||
      !reader.ReadStringMoqVarInt(request_error.reason_phrase)) {
    return absl::InvalidArgumentError("Message missing fields");
  }
  request_error.error_code = static_cast<RequestErrorCode>(error_code);
  request_error.retry_interval =
      (raw_interval == 0)
          ? std::nullopt
          : std::make_optional(
                quic::QuicTimeDelta::FromMilliseconds(raw_interval - 1));
  if (request_error.error_code == RequestErrorCode::kRedirect) {
    request_error.redirect.emplace();
    if (!reader.ReadStringMoqVarInt(request_error.redirect->connect_uri)) {
      return absl::InvalidArgumentError("Redirect missing connect URI");
    }
    if (perspective_ == quic::Perspective::IS_SERVER &&
        !request_error.redirect->connect_uri.empty()) {
      return absl::InvalidArgumentError(
          "Connect URI must be empty from client");
    }
    QUICHE_RETURN_IF_ERROR(
        ReadFullTrackName(reader, request_error.redirect->full_track_name));
  }
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return request_error;
}

absl::StatusOr<MoqtPublishDone> MoqtControlMessageParser::ProcessPublishDone(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtPublishDone publish_done;
  uint64_t value;
  if (!reader.ReadMoqVarInt(&value) ||
      !reader.ReadMoqVarInt(&publish_done.stream_count) ||
      !reader.ReadStringMoqVarInt(publish_done.error_reason)) {
    return absl::InvalidArgumentError("Message missing fields");
  }
  publish_done.status_code = static_cast<PublishDoneCode>(value);
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return publish_done;
}

absl::StatusOr<MoqtRequestUpdate>
MoqtControlMessageParser::ProcessRequestUpdate(absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtRequestUpdate request_update;
  if (!reader.ReadMoqVarInt(&request_update.request_id)) {
    return absl::InvalidArgumentError("Request ID missing");
  }
  QUICHE_ASSIGN_OR_RETURN(request_update.parameters,
                          ParseMessageParameters(reader));
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return request_update;
}

absl::StatusOr<MoqtPublishNamespace>
MoqtControlMessageParser::ProcessPublishNamespace(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtPublishNamespace publish_namespace;
  if (!reader.ReadMoqVarInt(&publish_namespace.request_id)) {
    return absl::InvalidArgumentError("Request ID missing");
  }
  QUICHE_RETURN_IF_ERROR(
      ReadTrackNamespace(reader, publish_namespace.track_namespace));
  QUICHE_ASSIGN_OR_RETURN(publish_namespace.parameters,
                          ParseMessageParameters(reader));
  if (!ParametersAllowedByMessage(publish_namespace.parameters,
                                  MoqtMessageType::kPublishNamespace)) {
    return absl::InvalidArgumentError(
        "Invalid PUBLISH_NAMESPACE message parameters");
  }
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return publish_namespace;
}

absl::StatusOr<MoqtNamespace> MoqtControlMessageParser::ProcessNamespace(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtNamespace _namespace;
  QUICHE_RETURN_IF_ERROR(
      ReadTrackNamespace(reader, _namespace.track_namespace_suffix));
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return _namespace;
}

absl::StatusOr<MoqtNamespaceDone>
MoqtControlMessageParser::ProcessNamespaceDone(absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtNamespaceDone namespace_done;
  QUICHE_RETURN_IF_ERROR(
      ReadTrackNamespace(reader, namespace_done.track_namespace_suffix));
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return namespace_done;
}

absl::StatusOr<MoqtPublishSkipped>
MoqtControlMessageParser::ProcessPublishSkipped(absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtPublishSkipped publish_skipped;
  QUICHE_RETURN_IF_ERROR(ReadFullTrackName(reader, publish_skipped.name));
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return publish_skipped;
}

absl::StatusOr<MoqtRequestOk> MoqtControlMessageParser::ProcessRequestOk(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtRequestOk request_ok;
  QUICHE_ASSIGN_OR_RETURN(request_ok.parameters,
                          ParseMessageParameters(reader));
  QUICHE_RETURN_IF_ERROR(ParseKeyValuePairList(reader, request_ok.properties));
  if (!request_ok.properties.Validate()) {
    return absl::InvalidArgumentError("Invalid REQUEST_OK track properties");
  }
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return request_ok;
}

absl::StatusOr<MoqtTrackStatus> MoqtControlMessageParser::ProcessTrackStatus(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtTrackStatus track_status;
  if (!reader.ReadMoqVarInt(&track_status.request_id)) {
    return absl::InvalidArgumentError("Failed to read request ID");
  }
  QUICHE_RETURN_IF_ERROR(
      ReadFullTrackName(reader, track_status.full_track_name));
  QUICHE_ASSIGN_OR_RETURN(track_status.parameters,
                          ParseMessageParameters(reader));
  if (!ParametersAllowedByMessage(track_status.parameters,
                                  MoqtMessageType::kTrackStatus)) {
    return absl::InvalidArgumentError(
        "Invalid TRACK_STATUS message parameters");
  }
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return track_status;
}

absl::StatusOr<MoqtGoAway> MoqtControlMessageParser::ProcessGoAway(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtGoAway goaway;
  uint64_t raw_timeout;
  if (!reader.ReadStringMoqVarInt(goaway.new_session_uri)) {
    return absl::InvalidArgumentError("Missing new session URI");
  }
  if (perspective_ == quic::Perspective::IS_SERVER &&
      !goaway.new_session_uri.empty()) {
    return absl::InvalidArgumentError(
        "New session URI must be empty from client");
  }
  if (goaway.new_session_uri.length() > kMaxNewSessionUriLength) {
    return absl::InvalidArgumentError("New session URI too long");
  }
  if (!reader.ReadMoqVarInt(&raw_timeout)) {
    return absl::InvalidArgumentError("Missing GOAWAY timeout");
  }
  goaway.timeout = quic::QuicTimeDelta::TryFromMilliseconds(raw_timeout)
                       .value_or(quic::QuicTimeDelta::Infinite());
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return goaway;
}

absl::StatusOr<MoqtSubscribeNamespace>
MoqtControlMessageParser::ProcessSubscribeNamespace(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtSubscribeNamespace subscribe_namespace;
  if (!reader.ReadMoqVarInt(&subscribe_namespace.request_id)) {
    return absl::InvalidArgumentError("Request ID missing");
  }
  QUICHE_RETURN_IF_ERROR(
      ReadTrackNamespace(reader, subscribe_namespace.track_namespace_prefix));
  QUICHE_ASSIGN_OR_RETURN(subscribe_namespace.parameters,
                          ParseMessageParameters(reader));
  if (!ParametersAllowedByMessage(subscribe_namespace.parameters,
                                  MoqtMessageType::kSubscribeNamespace)) {
    return absl::InvalidArgumentError(
        "Invalid SUBSCRIBE_NAMESPACE message parameters");
  }
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return subscribe_namespace;
}

absl::StatusOr<MoqtSubscribeTracks>
MoqtControlMessageParser::ProcessSubscribeTracks(absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtSubscribeTracks subscribe_tracks;
  if (!reader.ReadMoqVarInt(&subscribe_tracks.request_id)) {
    return absl::InvalidArgumentError("Request ID missing");
  }
  QUICHE_RETURN_IF_ERROR(
      ReadTrackNamespace(reader, subscribe_tracks.track_namespace_prefix));
  QUICHE_ASSIGN_OR_RETURN(subscribe_tracks.parameters,
                          ParseMessageParameters(reader));
  if (!ParametersAllowedByMessage(subscribe_tracks.parameters,
                                  MoqtMessageType::kSubscribeTracks)) {
    return absl::InvalidArgumentError(
        "Invalid SUBSCRIBE_TRACKS message parameters");
  }
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return subscribe_tracks;
}

absl::StatusOr<MoqtFetch> MoqtControlMessageParser::ProcessFetch(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtFetch fetch;
  uint64_t type;
  if (!reader.ReadMoqVarInt(&fetch.request_id) ||
      !reader.ReadMoqVarInt(&type)) {
    return absl::InvalidArgumentError("Message missing fields");
  }
  switch (static_cast<FetchType>(type)) {
    case FetchType::kAbsoluteJoining: {
      uint64_t joining_request_id;
      uint64_t joining_start;
      if (!reader.ReadMoqVarInt(&joining_request_id) ||
          !reader.ReadMoqVarInt(&joining_start)) {
        return absl::InvalidArgumentError(
            "Absolute joining parameters invalid");
      }
      fetch.fetch = JoiningFetchAbsolute{joining_request_id, joining_start};
      break;
    }
    case FetchType::kRelativeJoining: {
      uint64_t joining_request_id;
      uint64_t joining_start;
      if (!reader.ReadMoqVarInt(&joining_request_id) ||
          !reader.ReadMoqVarInt(&joining_start)) {
        return absl::InvalidArgumentError(
            "Relative joining parameters invalid");
      }
      fetch.fetch = JoiningFetchRelative{joining_request_id, joining_start};
      break;
    }
    case FetchType::kStandalone: {
      fetch.fetch = StandaloneFetch();
      StandaloneFetch& standalone_fetch =
          std::get<StandaloneFetch>(fetch.fetch);
      QUICHE_RETURN_IF_ERROR(
          ReadFullTrackName(reader, standalone_fetch.full_track_name));
      if (!reader.ReadMoqVarInt(&standalone_fetch.start_location.group) ||
          !reader.ReadMoqVarInt(&standalone_fetch.start_location.object) ||
          !reader.ReadMoqVarInt(&standalone_fetch.end_location.group) ||
          !reader.ReadMoqVarInt(&standalone_fetch.end_location.object)) {
        return absl::InvalidArgumentError(
            "Standalone fetch parameters invalid");
      }
      if (standalone_fetch.end_location.object == 0) {
        standalone_fetch.end_location.object = kMaxObjectId;
      } else {
        --standalone_fetch.end_location.object;
      }
      if (standalone_fetch.end_location < standalone_fetch.start_location) {
        return absl::InvalidArgumentError(
            "End object comes before start object in FETCH");
      }
      break;
    }
    default:
      return absl::InvalidArgumentError("Invalid FETCH type");
  }
  QUICHE_ASSIGN_OR_RETURN(fetch.parameters, ParseMessageParameters(reader));
  if (!ParametersAllowedByMessage(fetch.parameters, MoqtMessageType::kFetch)) {
    return absl::InvalidArgumentError("Invalid FETCH message parameters");
  }
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return fetch;
}

absl::StatusOr<MoqtFetchOk> MoqtControlMessageParser::ProcessFetchOk(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtFetchOk fetch_ok;
  uint8_t end_of_track;
  if (!reader.ReadUInt8(&end_of_track) ||
      !reader.ReadMoqVarInt(&fetch_ok.end_location.group) ||
      !reader.ReadMoqVarInt(&fetch_ok.end_location.object)) {
    return absl::InvalidArgumentError("Message missing fields");
  }
  if (end_of_track > 0x01) {
    return absl::InvalidArgumentError("Invalid end of track value in FETCH_OK");
  }
  if (fetch_ok.end_location.object == 0) {
    fetch_ok.end_location.object = kMaxObjectId;
  } else {
    --fetch_ok.end_location.object;
  }
  fetch_ok.end_of_track = end_of_track == 1;
  QUICHE_ASSIGN_OR_RETURN(fetch_ok.parameters, ParseMessageParameters(reader));
  if (!ParametersAllowedByMessage(fetch_ok.parameters,
                                  MoqtMessageType::kFetchOk)) {
    return absl::InvalidArgumentError("Invalid FETCH_OK message parameters");
  }
  QUICHE_RETURN_IF_ERROR(ParseKeyValuePairList(reader, fetch_ok.properties));
  if (!fetch_ok.properties.Validate()) {
    return absl::InvalidArgumentError("Invalid FETCH_OK track properties");
  }
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return fetch_ok;
}

absl::StatusOr<MoqtPublish> MoqtControlMessageParser::ProcessPublish(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtPublish publish;
  QUICHE_DCHECK(reader.PreviouslyReadPayload().empty());
  if (!reader.ReadMoqVarInt(&publish.request_id)) {
    return absl::InvalidArgumentError("Request ID missing");
  }
  QUICHE_RETURN_IF_ERROR(ReadFullTrackName(reader, publish.full_track_name));
  if (!reader.ReadMoqVarInt(&publish.track_alias)) {
    return absl::InvalidArgumentError("Track alias missing");
  }
  QUICHE_ASSIGN_OR_RETURN(publish.parameters, ParseMessageParameters(reader));
  if (!ParametersAllowedByMessage(publish.parameters,
                                  MoqtMessageType::kPublish)) {
    return absl::InvalidArgumentError("Invalid PUBLISH message parameters");
  }
  QUICHE_RETURN_IF_ERROR(ParseKeyValuePairList(reader, publish.properties));
  if (!publish.properties.Validate()) {
    return absl::InvalidArgumentError("Invalid PUBLISH track properties");
  }
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return publish;
}

absl::StatusOr<MoqtPublishStateNotify>
MoqtControlMessageParser::ProcessPublishStateNotify(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtPublishStateNotify publish_state_notify;
  QUICHE_ASSIGN_OR_RETURN(publish_state_notify.parameters,
                          ParseMessageParameters(reader));
  if (!ParametersAllowedByMessage(publish_state_notify.parameters,
                                  MoqtMessageType::kPublishStateNotify)) {
    return absl::InvalidArgumentError(
        "Invalid PUBLISH_STATE_NOTIFY message parameters");
  }
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return publish_state_notify;
}

absl::StatusOr<MoqtObjectAck> MoqtControlMessageParser::ProcessObjectAck(
    absl::string_view data) const {
  quic::QuicDataReader reader(data);
  MoqtObjectAck object_ack;
  uint64_t raw_delta;
  if (!reader.ReadMoqVarInt(&object_ack.group_id) ||
      !reader.ReadMoqVarInt(&object_ack.object_id) ||
      !reader.ReadMoqVarInt(&raw_delta)) {
    return absl::InvalidArgumentError("Message missing fields");
  }
  object_ack.delta_from_deadline = quic::QuicTimeDelta::FromMicroseconds(
      SignedVarintUnserializedForm(raw_delta));
  QUICHE_RETURN_IF_ERROR(CheckForTrailingData(reader));
  return object_ack;
}

absl::Status MoqtControlMessageParser::FillAndValidateSetupOptions(
    const KeyValuePairList& in, SetupOptions& out) const {
  QUICHE_RETURN_IF_ERROR(out.FromKeyValuePairList(in));
  MoqtError error = SetupOptionsAllowedByMessage(
      out, FlipPerspective(perspective_), uses_web_transport_);
  if (error != MoqtError::kNoError) {
    return MoqtErrorStatusWithCode("Setup option parsing error", error);
  }
  return absl::OkStatus();
}

MoqtDataParser::MoqtDataParser(MoqtStreamTypeParser type_parser,
                               MoqtDataParserVisitor* visitor)
    : stream_(*type_parser.stream()), visitor_(*visitor) {
  if (type_parser.stream_type().has_value()) {
    ProcessStreamType(*type_parser.stream_type());
  } else {
    next_input_ = kStreamType;
  }
}

void MoqtDataParser::ParseError(absl::string_view reason) {
  if (parsing_error_) {
    return;  // Don't send multiple parse errors.
  }
  next_input_ = kFailed;
  no_more_data_ = true;
  parsing_error_ = true;
  visitor_.OnParsingError(MoqtError::kProtocolViolation, reason);
}

absl::StatusOr<absl::string_view> ParseDatagram(absl::string_view data,
                                                MoqtObject& object_metadata,
                                                bool& use_default_priority) {
  uint64_t type_raw, object_status_raw;
  absl::string_view properties;
  quic::QuicDataReader reader(data);
  object_metadata = MoqtObject();
  if (!reader.ReadMoqVarInt(&type_raw)) {
    return absl::InvalidArgumentError("Failed to parse datagram");
  }
  if (type_raw == kPaddingDatagramType) {
    return absl::NotFoundError("Padding datagram");
  }
  std::optional<MoqtDatagramType> datagram_type =
      MoqtDatagramType::FromValue(type_raw);
  if (!datagram_type.has_value()) {
    return absl::InvalidArgumentError("Failed to parse datagram");
  }
  if (!reader.ReadMoqVarInt(&object_metadata.track_alias) ||
      !reader.ReadMoqVarInt(&object_metadata.group_id)) {
    return absl::InvalidArgumentError("Failed to parse datagram");
  }
  if (datagram_type->end_of_group()) {
    object_metadata.object_status = MoqtObjectStatus::kEndOfGroup;
    if (datagram_type->has_status()) {
      QUICHE_BUG(Moqt_invalid_datagram_type)
          << "Invalid datagram type: " << type_raw;
      return absl::InvalidArgumentError("Failed to parse datagram");
    }
  } else {
    object_metadata.object_status = MoqtObjectStatus::kNormal;
  }
  if (datagram_type->has_object_id()) {
    if (!reader.ReadMoqVarInt(&object_metadata.object_id)) {
      return absl::InvalidArgumentError("Failed to parse datagram");
    }
  } else {
    object_metadata.object_id = 0;
  }
  object_metadata.subgroup_id = std::nullopt;
  use_default_priority = datagram_type->has_default_priority();
  if (!use_default_priority &&
      !reader.ReadUInt8(&object_metadata.publisher_priority)) {
    return absl::InvalidArgumentError("Failed to parse datagram");
  }
  if (datagram_type->has_properties()) {
    if (!reader.ReadStringPieceMoqVarInt(&properties)) {
      return absl::InvalidArgumentError("Failed to parse datagram");
    }
    if (properties.empty()) {
      // This is a session error.
      return absl::InvalidArgumentError("Failed to parse datagram");
    }
    object_metadata.properties = std::string(properties);
  }
  if (datagram_type->has_status()) {
    object_metadata.payload_length = 0;
    if (!reader.ReadMoqVarInt(&object_status_raw)) {
      return absl::InvalidArgumentError("Failed to parse datagram");
    }
    object_metadata.object_status = IntegerToObjectStatus(object_status_raw);
    if (datagram_type->has_properties() &&
        object_metadata.object_status != MoqtObjectStatus::kNormal) {
      return absl::InvalidArgumentError("Datagram has status and properties");
    }
    return "";
  }
  absl::string_view payload = reader.ReadRemainingPayload();
  object_metadata.payload_length = payload.length();
  return payload;
}

void MoqtDataParser::ReadDataUntil(StopCondition stop_condition) {
  if (processing_) {
    QUICHE_BUG(MoqtDataParser_reentry)
        << "Calling ProcessData() when ProcessData() is already in progress.";
    return;
  }
  processing_ = true;
  auto on_return = absl::MakeCleanup([&] { processing_ = false; });

  State last_state = state();
  for (;;) {
    ParseNextItemFromStream();
    if (state() == last_state || no_more_data_ || stop_condition()) {
      break;
    }
    last_state = state();
  }
}

std::optional<uint64_t> MoqtDataParser::ReadMoqVarIntNoFin() {
  bool fin_read = false;
  std::optional<uint64_t> result = ReadMoqVarIntFromStream(stream_, fin_read);
  if (fin_read) {  // FIN received before a complete varint.
    ParseError("FIN after incomplete message");
    return std::nullopt;
  }
  return result;
}

std::optional<uint8_t> MoqtDataParser::ReadUint8NoFin() {
  char buffer[1];
  webtransport::Stream::ReadResult read_result =
      stream_.Read(absl::MakeSpan(buffer));
  if (read_result.bytes_read == 0) {
    return std::nullopt;
  }
  return absl::bit_cast<uint8_t>(buffer[0]);
}

MoqtDataParser::NextInput MoqtDataParser::AdvanceParserState() {
  if (type_.IsFetch()) {
    switch (next_input_) {
      case kStreamType:
        return kRequestId;
      case kRequestId:
        return kSerializationFlags;
      case kSerializationFlags:
        if (fetch_serialization_.has_group_id()) {
          return kGroupId;
        }
        [[fallthrough]];
      case kGroupId:
        if (fetch_serialization_.is_datagram()) {
          metadata_.subgroup_id = std::nullopt;
        } else if (!fetch_serialization_.end_of_non_existent_range() &&
                   !fetch_serialization_.end_of_unknown_range()) {
          // Object ranges do not set the subgroup.
          if (fetch_serialization_.has_subgroup_id()) {
            return kSubgroupId;
          }
          if (fetch_serialization_.prior_subgroup_id_plus_one()) {
            if (!metadata_.subgroup_id.has_value()) {
              ParseError("reference to subgroup ID of prior datagram");
              return kFailed;
            }
            ++(*metadata_.subgroup_id);
          } else if (fetch_serialization_.zero_subgroup_id()) {
            metadata_.subgroup_id = 0;
          } else if (!metadata_.subgroup_id.has_value()) {
            QUICHE_DCHECK(fetch_serialization_.prior_subgroup_id());
            ParseError("reference to subgroup ID of prior datagram");
            return kFailed;
          }
        }
        [[fallthrough]];
      case kSubgroupId:
        if (fetch_serialization_.has_object_id()) {
          return kObjectId;
        }
        if (!quiche::SafeIncrementBy<uint64_t>(metadata_.object_id, 1)) {
          ParseError("Integer overflow when parsing object ID");
          return kFailed;
        }
        last_object_id_ = metadata_.object_id;
        [[fallthrough]];
      case kObjectId:
        if (fetch_serialization_.end_of_non_existent_range() ||
            fetch_serialization_.end_of_unknown_range()) {
          return kSerializationFlags;
        }
        if (fetch_serialization_.has_priority()) {
          return kPublisherPriority;
        }
        [[fallthrough]];
      case kPublisherPriority:
        if (fetch_serialization_.has_properties()) {
          return kPropertiesSize;
        }
        metadata_.properties = "";
        return kObjectPayloadLength;
      case kPropertiesBody:
        return kObjectPayloadLength;
      case kData:
        return kSerializationFlags;
      case kTrackAlias:
      case kObjectPayloadLength:
      case kStatus:
      case kFailed:
      case kPropertiesSize:
      case kPadding:
        QUICHE_NOTREACHED();
        return next_input_;
    }
  }
  switch (next_input_) {
    // The state table is factored into a separate function (rather than
    // inlined) in order to separate the order of elements from the way they are
    // parsed.
    case kStreamType:
      return kTrackAlias;
    case kTrackAlias:
      return kGroupId;
    case kGroupId:
      if (type_.IsSubgroupPresent()) {
        return kSubgroupId;
      }
      if (type_.SubgroupIsZero()) {
        metadata_.subgroup_id = 0;
      }
      [[fallthrough]];
    case kSubgroupId:
      if (!type_.HasDefaultPriority()) {
        return kPublisherPriority;
      }
      metadata_.publisher_priority = default_publisher_priority_;
      [[fallthrough]];
    case kPublisherPriority:
      return kObjectId;
    case kObjectId:
      if (num_objects_read_ == 0 && type_.SubgroupIsFirstObjectId()) {
        metadata_.subgroup_id = metadata_.object_id;
      }
      if (type_.IsSubgroup()) {
        metadata_.first_object_in_subgroup =
            type_.HasFirstObject() && num_objects_read_ == 0;
      }
      if (type_.ArePropertiesPresent()) {
        return kPropertiesSize;
      }
      [[fallthrough]];
    case kPropertiesBody:
      return kObjectPayloadLength;
    case kStatus:
    case kData:
      return kObjectId;
    case kRequestId:
    case kSerializationFlags:
    case kPropertiesSize:
    case kObjectPayloadLength:
    case kPadding:
    case kFailed:
      // Other transitions are either Fetch-only or handled in
      // ParseNextItemFromStream.
      QUICHE_NOTREACHED();
      return next_input_;
  }
}

void MoqtDataParser::ParseNextItemFromStream() {
  if (CheckForFinWithoutData()) {
    return;
  }
  switch (next_input_) {
    case kStreamType: {
      // TODO(vasilvv): Handle padding streams (which are allowed to FIN
      // immediately after type, unlike all other types of streams).
      std::optional<uint64_t> value_read = ReadMoqVarIntNoFin();
      if (!value_read.has_value()) {
        return;
      }
      ProcessStreamType(*value_read);
      return;
    }

    case kRequestId:
    case kTrackAlias: {
      std::optional<uint64_t> value_read = ReadMoqVarIntNoFin();
      if (value_read.has_value()) {
        metadata_.track_alias = *value_read;
        next_input_ = AdvanceParserState();
      }
      return;
    }

    case kSerializationFlags: {
      std::optional<uint64_t> value_read = ReadMoqVarIntNoFin();
      if (value_read.has_value()) {
        std::optional<MoqtFetchSerialization> serialization =
            MoqtFetchSerialization::FromValue(*value_read);
        if (!serialization.has_value()) {
          ParseError("Invalid serialization flags");
          return;
        }
        if (num_objects_read_ == 0 && (!serialization->has_object_id() ||
                                       !serialization->has_group_id())) {
          ParseError("Invalid serialization flags for first object");
          return;
        }
        if (!non_fetch_range_object_read_ &&
            !serialization->end_of_non_existent_range() &&
            !serialization->end_of_unknown_range() &&
            (serialization->prior_subgroup_id() ||
             serialization->prior_subgroup_id_plus_one() ||
             !serialization->has_priority())) {
          ParseError("Invalid reference to prior object");
          return;
        }
        fetch_serialization_ = *serialization;
        next_input_ = AdvanceParserState();
      }
      return;
    }

    case kGroupId: {
      if (type_.IsFetch() && !group_order_.has_value()) {
        // Check before reading the group ID to avoid wrecking parser state.
        QUICHE_BUG(MoqtDataParser_no_group_order)
            << "Data parser group order not set";
        return;
      }
      std::optional<uint64_t> value_read = ReadMoqVarIntNoFin();
      if (!value_read.has_value()) {
        return;
      }
      if (type_.IsFetch() && last_group_id_.has_value()) {
        // Diff-encoded group ID.
        std::optional<uint64_t> new_group_id;
        if (!group_order_.has_value()) {
          // Exit to satisfy ClangTidy.
          return;
        }
        if (*group_order_ == MoqtGroupOrder::kAscending) {
          new_group_id =
              quiche::SafeSum<uint64_t>({*value_read, *last_group_id_, 1});
        } else {
          new_group_id = (*value_read < *last_group_id_)
                             ? (*last_group_id_ - *value_read - 1)
                             : std::optional<uint64_t>();
        }
        if (!new_group_id.has_value()) {
          ParseError("Integer overflow when parsing group ID");
          return;
        }
        metadata_.group_id = *new_group_id;
      } else {
        metadata_.group_id = *value_read;
      }
      last_group_id_ = metadata_.group_id;
      next_input_ = AdvanceParserState();
      return;
    }

    case kSubgroupId: {
      std::optional<uint64_t> value_read = ReadMoqVarIntNoFin();
      if (value_read.has_value()) {
        metadata_.subgroup_id = *value_read;
        next_input_ = AdvanceParserState();
      }
      return;
    }

    case kPublisherPriority: {
      std::optional<uint8_t> value_read = ReadUint8NoFin();
      if (value_read.has_value()) {
        metadata_.publisher_priority = *value_read;
        next_input_ = AdvanceParserState();
      }
      return;
    }

    case kObjectId: {
      bool fin_read = false;
      std::optional<uint64_t> value_read =
          ReadMoqVarIntFromStream(stream_, fin_read);
      if (!value_read.has_value()) {
        if (fin_read) {
          ParseError("FIN received at an unexpected point in the stream");
        }
        return;
      }
      if (last_object_id_.has_value() &&
          (type_.IsSubgroup() || !fetch_serialization_.has_group_id())) {
        if (type_.IsFetch() && *value_read == 0) {
          ParseError("Duplicate object ID in FETCH stream");
          return;
        }
        std::optional<uint64_t> new_object_id = quiche::SafeSum<uint64_t>(
            {*value_read, *last_object_id_,
             type_.IsSubgroup() ? uint64_t{1} : uint64_t{0}});
        if (!new_object_id.has_value()) {
          ParseError("Integer overflow when parsing object ID");
          return;
        }
        metadata_.object_id = *new_object_id;
      } else {
        metadata_.object_id = *value_read;
      }
      last_object_id_ = metadata_.object_id;
      if (type_.IsFetch() &&
          (fetch_serialization_.end_of_non_existent_range() ||
           fetch_serialization_.end_of_unknown_range())) {
        metadata_.object_status =
            static_cast<MoqtObjectStatus>(fetch_serialization_.value());
        visitor_.OnObjectMessage(metadata_, "", /*end_of_message=*/true);
        ++num_objects_read_;
        if (fin_read) {
          visitor_.OnFin();
          no_more_data_ = true;
          return;
        }
      } else if (fin_read) {
        ParseError("FIN received at an unexpected point in the stream");
        return;
      }
      next_input_ = AdvanceParserState();
      return;
    }

    case kPropertiesSize: {
      std::optional<uint64_t> value_read = ReadMoqVarIntNoFin();
      if (value_read.has_value()) {
        metadata_.properties.clear();
        payload_length_remaining_ = *value_read;
        next_input_ =
            (value_read == 0) ? kObjectPayloadLength : kPropertiesBody;
      }
      return;
    }

    case kObjectPayloadLength: {
      std::optional<uint64_t> value_read = ReadMoqVarIntNoFin();
      if (value_read.has_value()) {
        metadata_.payload_length = *value_read;
        payload_length_remaining_ = *value_read;
        if (metadata_.payload_length > 0) {
          metadata_.object_status = MoqtObjectStatus::kNormal;
          next_input_ = kData;
        } else {
          next_input_ = type_.IsFetch() ? kSerializationFlags : kStatus;
        }
      }
      return;
    }

    case kStatus: {
      bool fin_read = false;
      std::optional<uint64_t> value_read =
          ReadMoqVarIntFromStream(stream_, fin_read);
      if (value_read.has_value()) {
        metadata_.object_status = IntegerToObjectStatus(*value_read);
        if (metadata_.object_status == MoqtObjectStatus::kInvalidObjectStatus) {
          ParseError("Invalid object status provided");
          return;
        }
        if (metadata_.object_status != MoqtObjectStatus::kNormal &&
            !metadata_.properties.empty()) {
          ParseError("Object has status and properties");
          return;
        }
        ++num_objects_read_;
        // TODO(martinduke): If contains_end_of_group_ && fin_read, the track is
        // malformed. There is no API to signal this to the session yet, but the
        // contains_end_of_group_ logic is likely to substantially change in the
        // spec. Don't bother to signal this for now; just ignore that the
        // stream was supposed to conclude with kEndOfGroup and end it with the
        // encoded status instead.
        non_fetch_range_object_read_ = true;
        visitor_.OnObjectMessage(metadata_, "", /*end_of_message=*/true);
        next_input_ = AdvanceParserState();
      }
      if (fin_read) {
        visitor_.OnFin();
        no_more_data_ = true;
        return;
      }
      return;
    }

    case kPropertiesBody:
    case kData: {
      while (payload_length_remaining_ > 0) {
        webtransport::Stream::PeekResult peek_result =
            stream_.PeekNextReadableRegion();
        if (!peek_result.has_data()) {
          return;
        }
        size_t chunk_size =
            std::min(payload_length_remaining_, peek_result.peeked_data.size());
        payload_length_remaining_ -= chunk_size;
        bool done = payload_length_remaining_ == 0;
        if (next_input_ == kData) {
          no_more_data_ = peek_result.all_data_received &&
                          chunk_size == stream_.ReadableBytes();
          if (!done && no_more_data_) {
            ParseError("FIN received at an unexpected point in the stream");
            return;
          }
          if (contains_end_of_group_ && no_more_data_) {
            // It's OK to change status mid-object because it's a convenience
            // signal for the parser, not immutable metadata.
            metadata_.object_status = MoqtObjectStatus::kEndOfGroup;
          }
          non_fetch_range_object_read_ = true;
          visitor_.OnObjectMessage(
              metadata_, peek_result.peeked_data.substr(0, chunk_size), done);
          if (done) {
            if (no_more_data_) {
              visitor_.OnFin();
            }
            ++num_objects_read_;
            next_input_ = AdvanceParserState();
          }
          if (stream_.SkipBytes(chunk_size) && !no_more_data_) {
            // Although there was no FIN, SkipBytes() can return true if the
            // stream is reset, probably because OnObjectMessage() caused
            // something to happen to the stream or the session.
            no_more_data_ = true;
            return;
          }
        } else {
          absl::StrAppend(&metadata_.properties,
                          peek_result.peeked_data.substr(0, chunk_size));
          if (stream_.SkipBytes(chunk_size)) {
            ParseError("FIN received at an unexpected point in the stream");
            no_more_data_ = true;
            return;
          }
          if (done) {
            next_input_ = AdvanceParserState();
          }
        }
      }
      return;
    }

    case kPadding:
      no_more_data_ |= stream_.SkipBytes(stream_.ReadableBytes());
      return;

    case kFailed:
      return;
  }
}

void MoqtDataParser::ReadAllData() {
  ReadDataUntil(+[]() { return false; });
}

void MoqtDataParser::ReadStreamType() {
  return ReadDataUntil([this]() { return next_input_ != kStreamType; });
}

void MoqtDataParser::ReadTrackAlias() {
  return ReadDataUntil([this]() { return next_input_ > kTrackAlias; });
}

void MoqtDataParser::ReadAtMostOneObject() {
  const size_t num_objects_read_initial = num_objects_read_;
  return ReadDataUntil(
      [&]() { return num_objects_read_ != num_objects_read_initial; });
}

bool MoqtDataParser::CheckForFinWithoutData() {
  if (no_more_data_) {
    return true;
  }
  if (!stream_.PeekNextReadableRegion().fin_next) {
    return false;
  }
  no_more_data_ = true;
  const bool valid_state =
      payload_length_remaining_ == 0 &&
      ((type_.IsSubgroup() && next_input_ == kObjectId) ||
       (type_.IsFetch() && next_input_ == kSerializationFlags));
  if (!valid_state) {
    ParseError("FIN received at an unexpected point in the stream");
    return true;
  }
  if (contains_end_of_group_ && last_object_id_.has_value() &&
      *last_object_id_ < kMaxObjectId &&
      metadata_.object_status == MoqtObjectStatus::kNormal) {
    // Send a synthetic END_OF_GROUP object.
    MoqtObject object = {
        .track_alias = metadata_.track_alias,
        .group_id = metadata_.group_id,
        .object_id = *last_object_id_ + 1,
        .publisher_priority = metadata_.publisher_priority,
        .properties = "",
        .object_status = MoqtObjectStatus::kEndOfGroup,
        .subgroup_id = metadata_.subgroup_id,
        .first_object_in_subgroup = false,
        .payload_length = 0,
    };
    ++num_objects_read_;
    visitor_.OnObjectMessage(object, "", /*end_of_message=*/true);
  }
  visitor_.OnFin();
  return stream_.SkipBytes(0);
}

void MoqtDataParser::ProcessStreamType(uint64_t raw_type) {
  std::optional<MoqtDataStreamType> type =
      MoqtDataStreamType::FromValue(raw_type);
  if (!type.has_value()) {
    ParseError("Invalid stream type supplied");
    return;
  }
  type_ = *type;
  if (type_.EndOfGroupInStream()) {
    contains_end_of_group_ = true;
  }
  next_input_ = AdvanceParserState();
}

}  // namespace moqt
