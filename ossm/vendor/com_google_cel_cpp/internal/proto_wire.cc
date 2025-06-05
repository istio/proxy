// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/proto_wire.h"

#include <limits>
#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace cel::internal {

bool SkipLengthValue(absl::Cord& data, ProtoWireType type) {
  switch (type) {
    case ProtoWireType::kVarint:
      if (auto result = VarintDecode<uint64_t>(data);
          ABSL_PREDICT_TRUE(result.has_value())) {
        data.RemovePrefix(result->size_bytes);
        return true;
      }
      return false;
    case ProtoWireType::kFixed64:
      if (ABSL_PREDICT_FALSE(data.size() < 8)) {
        return false;
      }
      data.RemovePrefix(8);
      return true;
    case ProtoWireType::kLengthDelimited:
      if (auto result = VarintDecode<uint32_t>(data);
          ABSL_PREDICT_TRUE(result.has_value())) {
        if (ABSL_PREDICT_TRUE(data.size() - result->size_bytes >=
                              result->value)) {
          data.RemovePrefix(result->size_bytes + result->value);
          return true;
        }
      }
      return false;
    case ProtoWireType::kFixed32:
      if (ABSL_PREDICT_FALSE(data.size() < 4)) {
        return false;
      }
      data.RemovePrefix(4);
      return true;
    case ProtoWireType::kStartGroup:
      ABSL_FALLTHROUGH_INTENDED;
    case ProtoWireType::kEndGroup:
      ABSL_FALLTHROUGH_INTENDED;
    default:
      return false;
  }
}

absl::StatusOr<ProtoWireTag> ProtoWireDecoder::ReadTag() {
  ABSL_DCHECK(!tag_.has_value());
  auto tag = internal::VarintDecode<uint32_t>(data_);
  if (ABSL_PREDICT_FALSE(!tag.has_value())) {
    return absl::DataLossError(
        absl::StrCat("malformed tag encountered decoding ", message_));
  }
  auto field = internal::DecodeProtoWireTag(tag->value);
  if (ABSL_PREDICT_FALSE(!field.has_value())) {
    return absl::DataLossError(
        absl::StrCat("invalid wire type or field number encountered decoding ",
                     message_, ": ", static_cast<std::string>(data_)));
  }
  data_.RemovePrefix(tag->size_bytes);
  tag_.emplace(*field);
  return *field;
}

absl::Status ProtoWireDecoder::SkipLengthValue() {
  ABSL_DCHECK(tag_.has_value());
  if (ABSL_PREDICT_FALSE(!internal::SkipLengthValue(data_, tag_->type()))) {
    return absl::DataLossError(
        absl::StrCat("malformed length or value encountered decoding field ",
                     tag_->field_number(), " of ", message_));
  }
  tag_.reset();
  return absl::OkStatus();
}

absl::StatusOr<absl::Cord> ProtoWireDecoder::ReadLengthDelimited() {
  ABSL_DCHECK(tag_.has_value() &&
              tag_->type() == ProtoWireType::kLengthDelimited);
  auto length = internal::VarintDecode<uint32_t>(data_);
  if (ABSL_PREDICT_FALSE(!length.has_value())) {
    return absl::DataLossError(
        absl::StrCat("malformed length encountered decoding field ",
                     tag_->field_number(), " of ", message_));
  }
  data_.RemovePrefix(length->size_bytes);
  if (ABSL_PREDICT_FALSE(data_.size() < length->value)) {
    return absl::DataLossError(absl::StrCat(
        "out of range length encountered decoding field ", tag_->field_number(),
        " of ", message_, ": ", length->value));
  }
  auto result = data_.Subcord(0, length->value);
  data_.RemovePrefix(length->value);
  tag_.reset();
  return result;
}

absl::Status ProtoWireEncoder::WriteTag(ProtoWireTag tag) {
  ABSL_DCHECK(!tag_.has_value());
  if (ABSL_PREDICT_FALSE(tag.field_number() == 0)) {
    // Cannot easily add test coverage as we assert during debug builds that
    // ProtoWireTag is valid upon construction.
    return absl::InvalidArgumentError(
        absl::StrCat("invalid field number encountered encoding ", message_));
  }
  if (ABSL_PREDICT_FALSE(!ProtoWireTypeIsValid(tag.type()))) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid wire type encountered encoding field ",
                     tag.field_number(), " of ", message_));
  }
  VarintEncode(static_cast<uint32_t>(tag), data_);
  tag_.emplace(tag);
  return absl::OkStatus();
}

absl::Status ProtoWireEncoder::WriteLengthDelimited(absl::Cord data) {
  ABSL_DCHECK(tag_.has_value() &&
              tag_->type() == ProtoWireType::kLengthDelimited);
  if (ABSL_PREDICT_FALSE(data.size() > std::numeric_limits<uint32_t>::max())) {
    return absl::InvalidArgumentError(
        absl::StrCat("out of range length encountered encoding field ",
                     tag_->field_number(), " of ", message_));
  }
  VarintEncode(static_cast<uint32_t>(data.size()), data_);
  data_.Append(std::move(data));
  tag_.reset();
  return absl::OkStatus();
}

absl::Status ProtoWireEncoder::WriteLengthDelimited(absl::string_view data) {
  ABSL_DCHECK(tag_.has_value() &&
              tag_->type() == ProtoWireType::kLengthDelimited);
  if (ABSL_PREDICT_FALSE(data.size() > std::numeric_limits<uint32_t>::max())) {
    return absl::InvalidArgumentError(
        absl::StrCat("out of range length encountered encoding field ",
                     tag_->field_number(), " of ", message_));
  }
  VarintEncode(static_cast<uint32_t>(data.size()), data_);
  data_.Append(data);
  tag_.reset();
  return absl::OkStatus();
}

}  // namespace cel::internal
