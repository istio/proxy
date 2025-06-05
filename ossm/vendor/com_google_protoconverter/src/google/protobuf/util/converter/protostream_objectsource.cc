// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/protobuf/util/converter/protostream_objectsource.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/base/call_once.h"
#include "absl/base/casts.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/stubs/status_macros.h"
#include "google/protobuf/stubs/strutil.h"
#include "google/protobuf/unknown_field_set.h"
#include "google/protobuf/util/converter/constants.h"
#include "google/protobuf/util/converter/field_mask_utility.h"
#include "google/protobuf/util/converter/utility.h"
#include "google/protobuf/wire_format.h"
#include "google/protobuf/wire_format_lite.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {
namespace {
using ::google::protobuf::internal::WireFormat;
using ::google::protobuf::internal::WireFormatLite;

static int kDefaultMaxRecursionDepth = 64;

// Finds a field with the given number. nullptr if none found.
const google::protobuf::Field* FindFieldByNumber(
    const google::protobuf::Type& type, int number);

// Returns true if the field is packable.
bool IsPackable(const google::protobuf::Field& field);

// Finds an enum value with the given number. nullptr if none found.
const google::protobuf::EnumValue* FindEnumValueByNumber(
    const google::protobuf::Enum& tech_enum, int number);

// Utility function to format nanos.
std::string FormatNanos(uint32_t nanos, bool with_trailing_zeros);

absl::StatusOr<std::string> MapKeyDefaultValueAsString(
    const google::protobuf::Field& field) {
  switch (field.kind()) {
    case google::protobuf::Field::TYPE_BOOL:
      return std::string("false");
    case google::protobuf::Field::TYPE_INT32:
    case google::protobuf::Field::TYPE_INT64:
    case google::protobuf::Field::TYPE_UINT32:
    case google::protobuf::Field::TYPE_UINT64:
    case google::protobuf::Field::TYPE_SINT32:
    case google::protobuf::Field::TYPE_SINT64:
    case google::protobuf::Field::TYPE_SFIXED32:
    case google::protobuf::Field::TYPE_SFIXED64:
    case google::protobuf::Field::TYPE_FIXED32:
    case google::protobuf::Field::TYPE_FIXED64:
      return std::string("0");
    case google::protobuf::Field::TYPE_STRING:
      return std::string();
    default:
      return absl::InternalError("Invalid map key type.");
  }
}
}  // namespace

ProtoStreamObjectSource::ProtoStreamObjectSource(
    io::CodedInputStream* stream, TypeResolver* type_resolver,
    const google::protobuf::Type& type, const RenderOptions& render_options)
    : stream_(stream),
      typeinfo_(TypeInfo::NewTypeInfo(type_resolver)),
      own_typeinfo_(true),
      type_(type),
      render_options_(render_options),
      recursion_depth_(0),
      max_recursion_depth_(kDefaultMaxRecursionDepth) {
  ABSL_DLOG_IF(FATAL, stream == nullptr) << "Input stream is nullptr.";
}

ProtoStreamObjectSource::ProtoStreamObjectSource(
    io::CodedInputStream* stream, const TypeInfo* typeinfo,
    const google::protobuf::Type& type, const RenderOptions& render_options)
    : stream_(stream),
      typeinfo_(typeinfo),
      own_typeinfo_(false),
      type_(type),
      render_options_(render_options),
      recursion_depth_(0),
      max_recursion_depth_(kDefaultMaxRecursionDepth) {
  ABSL_DLOG_IF(FATAL, stream == nullptr) << "Input stream is nullptr.";
}

ProtoStreamObjectSource::~ProtoStreamObjectSource() {
  if (own_typeinfo_) {
    delete typeinfo_;
  }
}

absl::Status ProtoStreamObjectSource::NamedWriteTo(absl::string_view name,
                                                   ObjectWriter* ow) const {
  return WriteMessage(type_, name, 0, true, ow);
}

const google::protobuf::Field*
ProtoStreamObjectSource::FindAndVerifyFieldHelper(
    const google::protobuf::Type& type, uint32_t tag) const {
  // Lookup the new field in the type by tag number.
  const google::protobuf::Field* field = FindFieldByNumber(type, tag >> 3);
  // Verify if the field corresponds to the wire type in tag.
  // If there is any discrepancy, mark the field as not found.
  if (field != nullptr) {
    WireFormatLite::WireType expected_type =
        WireFormatLite::WireTypeForFieldType(
            static_cast<WireFormatLite::FieldType>(field->kind()));
    WireFormatLite::WireType actual_type = WireFormatLite::GetTagWireType(tag);
    if (actual_type != expected_type &&
        (!IsPackable(*field) ||
         actual_type != WireFormatLite::WIRETYPE_LENGTH_DELIMITED)) {
      field = nullptr;
    }
  }
  return field;
}

const google::protobuf::Field* ProtoStreamObjectSource::FindAndVerifyField(
    const google::protobuf::Type& type, uint32_t tag) const {
  return FindAndVerifyFieldHelper(type, tag);
}

absl::Status ProtoStreamObjectSource::WriteMessage(
    const google::protobuf::Type& type, absl::string_view name,
    const uint32_t end_tag, bool include_start_and_end,
    ObjectWriter* ow) const {
  const TypeRenderer* type_renderer = FindTypeRenderer(type.name());
  if (type_renderer != nullptr) {
    return (*type_renderer)(this, type, name, ow);
  }

  const google::protobuf::Field* field = nullptr;
  std::string field_name;
  // last_tag set to dummy value that is different from tag.
  uint32_t tag = stream_->ReadTag(), last_tag = tag + 1;
  UnknownFieldSet unknown_fields;

  if (include_start_and_end) {
    ow->StartObject(name);
  }
  while (tag != end_tag && tag != 0) {
    if (tag != last_tag) {  // Update field only if tag is changed.
      last_tag = tag;
      field = FindAndVerifyField(type, tag);
      if (field != nullptr) {
        if (render_options_.preserve_proto_field_names) {
          field_name = field->name();
        } else {
          field_name = field->json_name();
        }
      }
    }
    if (field == nullptr) {
      // If we didn't find a field, skip this unknown tag.
      // TODO(wpoon): Check return boolean value.
      WireFormat::SkipField(stream_, tag, nullptr);
      tag = stream_->ReadTag();
      continue;
    }

    if (field->cardinality() == google::protobuf::Field::CARDINALITY_REPEATED) {
      if (IsMap(*field)) {
        ow->StartObject(field_name);
        ASSIGN_OR_RETURN(tag, RenderMap(field, field_name, tag, ow));
        ow->EndObject();
      } else {
        ASSIGN_OR_RETURN(tag, RenderList(field, field_name, tag, ow));
      }
    } else {
      // Render the field.
      RETURN_IF_ERROR(RenderField(field, field_name, ow));
      tag = stream_->ReadTag();
    }
  }

  if (include_start_and_end) {
    ow->EndObject();
  }
  return absl::Status();
}

absl::StatusOr<uint32_t> ProtoStreamObjectSource::RenderList(
    const google::protobuf::Field* field, absl::string_view name,
    uint32_t list_tag, ObjectWriter* ow) const {
  uint32_t tag_to_return = 0;
  ow->StartList(name);
  if (IsPackable(*field) &&
      list_tag ==
          WireFormatLite::MakeTag(field->number(),
                                  WireFormatLite::WIRETYPE_LENGTH_DELIMITED)) {
    RETURN_IF_ERROR(RenderPacked(field, ow));
    // Since packed fields have a single tag, read another tag from stream to
    // return.
    tag_to_return = stream_->ReadTag();
  } else {
    do {
      RETURN_IF_ERROR(RenderField(field, "", ow));
    } while ((tag_to_return = stream_->ReadTag()) == list_tag);
  }
  ow->EndList();
  return tag_to_return;
}

absl::StatusOr<uint32_t> ProtoStreamObjectSource::RenderMap(
    const google::protobuf::Field* field, absl::string_view /* name */,
    uint32_t list_tag, ObjectWriter* ow) const {
  const google::protobuf::Type* field_type =
      typeinfo_->GetTypeByTypeUrl(field->type_url());
  uint32_t tag_to_return = 0;
  do {
    // Render map entry message type.
    uint32_t buffer32;
    stream_->ReadVarint32(&buffer32);  // message length
    int old_limit = stream_->PushLimit(buffer32);
    std::string map_key;
    for (uint32_t tag = stream_->ReadTag(); tag != 0;
         tag = stream_->ReadTag()) {
      const google::protobuf::Field* map_entry_field =
          FindAndVerifyFieldHelper(*field_type, tag);
      if (map_entry_field == nullptr) {
        WireFormat::SkipField(stream_, tag, nullptr);
        continue;
      }
      // Map field numbers are key = 1 and value = 2
      if (map_entry_field->number() == 1) {
        map_key = ReadFieldValueAsString(*map_entry_field);
      } else if (map_entry_field->number() == 2) {
        if (map_key.empty()) {
          // An absent map key is treated as the default.
          const google::protobuf::Field* key_field =
              FindFieldByNumber(*field_type, 1);
          if (key_field == nullptr) {
            // The Type info for this map entry is incorrect. It should always
            // have a field named "key" and with field number 1.
            return absl::InternalError("Invalid map entry.");
          }
          ASSIGN_OR_RETURN(map_key, MapKeyDefaultValueAsString(*key_field));
        }
        RETURN_IF_ERROR(RenderField(map_entry_field, map_key, ow));
      } else {
        // The Type info for this map entry is incorrect. It should contain
        // exactly two fields with field number 1 and 2.
        return absl::InternalError("Invalid map entry.");
      }
    }
    stream_->PopLimit(old_limit);
  } while ((tag_to_return = stream_->ReadTag()) == list_tag);
  return tag_to_return;
}

absl::Status ProtoStreamObjectSource::RenderPacked(
    const google::protobuf::Field* field, ObjectWriter* ow) const {
  uint32_t length;
  stream_->ReadVarint32(&length);
  int old_limit = stream_->PushLimit(length);
  while (stream_->BytesUntilLimit() > 0) {
    RETURN_IF_ERROR(RenderField(field, absl::string_view(), ow));
  }
  stream_->PopLimit(old_limit);
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderTimestamp(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& type,
    absl::string_view field_name, ObjectWriter* ow) {
  std::pair<int64_t, int32_t> p = os->ReadSecondsAndNanos(type);
  int64_t seconds = p.first;
  int32_t nanos = p.second;
  if (seconds > kTimestampMaxSeconds || seconds < kTimestampMinSeconds) {
    return absl::InternalError(absl::StrCat(
        "Timestamp seconds exceeds limit for field: ", field_name));
  }

  if (nanos < 0 || nanos >= kNanosPerSecond) {
    return absl::InternalError(
        absl::StrCat("Timestamp nanos exceeds limit for field: ", field_name));
  }

  absl::Time tm = absl::FromUnixSeconds(seconds);
  std::string formatted_seconds =
      absl::FormatTime(kRfc3339TimeFormat, tm, absl::UTCTimeZone());
  std::string formatted_time = absl::StrFormat(
      "%s%sZ", formatted_seconds.c_str(), FormatNanos(nanos, false).c_str());
  ow->RenderString(field_name, formatted_time);
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderDuration(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& type,
    absl::string_view field_name, ObjectWriter* ow) {
  std::pair<int64_t, int32_t> p = os->ReadSecondsAndNanos(type);
  int64_t seconds = p.first;
  int32_t nanos = p.second;
  if (seconds > kDurationMaxSeconds || seconds < kDurationMinSeconds) {
    return absl::InternalError(
        absl::StrCat("Duration seconds exceeds limit for field: ", field_name));
  }

  if (nanos <= -kNanosPerSecond || nanos >= kNanosPerSecond) {
    return absl::InternalError(
        absl::StrCat("Duration nanos exceeds limit for field: ", field_name));
  }

  std::string sign = "";
  if (seconds < 0) {
    if (nanos > 0) {
      return absl::InternalError(
          absl::StrCat("Duration nanos is non-negative, but seconds is "
                       "negative for field: ",
                       field_name));
    }
    sign = "-";
    seconds = -seconds;
    nanos = -nanos;
  } else if (seconds == 0 && nanos < 0) {
    sign = "-";
    nanos = -nanos;
  }
  std::string formatted_duration = absl::StrFormat(
      "%s%lld%ss", sign.c_str(), static_cast<long long>(seconds),  // NOLINT
      FormatNanos(nanos, false).c_str());
  ow->RenderString(field_name, formatted_duration);
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderDouble(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& /*type*/,
    absl::string_view field_name, ObjectWriter* ow) {
  uint32_t tag = os->stream_->ReadTag();
  uint64_t buffer64 = 0;  // default value of Double wrapper value
  if (tag != 0) {
    os->stream_->ReadLittleEndian64(&buffer64);
    os->stream_->ReadTag();
  }
  ow->RenderDouble(field_name, absl::bit_cast<double>(buffer64));
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderFloat(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& /*type*/,
    absl::string_view field_name, ObjectWriter* ow) {
  uint32_t tag = os->stream_->ReadTag();
  uint32_t buffer32 = 0;  // default value of Float wrapper value
  if (tag != 0) {
    os->stream_->ReadLittleEndian32(&buffer32);
    os->stream_->ReadTag();
  }
  ow->RenderFloat(field_name, absl::bit_cast<float>(buffer32));
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderInt64(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& /*type*/,
    absl::string_view field_name, ObjectWriter* ow) {
  uint32_t tag = os->stream_->ReadTag();
  uint64_t buffer64 = 0;  // default value of Int64 wrapper value
  if (tag != 0) {
    os->stream_->ReadVarint64(&buffer64);
    os->stream_->ReadTag();
  }
  ow->RenderInt64(field_name, absl::bit_cast<int64_t>(buffer64));
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderUInt64(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& /*type*/,
    absl::string_view field_name, ObjectWriter* ow) {
  uint32_t tag = os->stream_->ReadTag();
  uint64_t buffer64 = 0;  // default value of UInt64 wrapper value
  if (tag != 0) {
    os->stream_->ReadVarint64(&buffer64);
    os->stream_->ReadTag();
  }
  ow->RenderUint64(field_name, absl::bit_cast<uint64_t>(buffer64));
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderInt32(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& /*type*/,
    absl::string_view field_name, ObjectWriter* ow) {
  uint32_t tag = os->stream_->ReadTag();
  uint32_t buffer32 = 0;  // default value of Int32 wrapper value
  if (tag != 0) {
    os->stream_->ReadVarint32(&buffer32);
    os->stream_->ReadTag();
  }
  ow->RenderInt32(field_name, absl::bit_cast<int32_t>(buffer32));
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderUInt32(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& /*type*/,
    absl::string_view field_name, ObjectWriter* ow) {
  uint32_t tag = os->stream_->ReadTag();
  uint32_t buffer32 = 0;  // default value of UInt32 wrapper value
  if (tag != 0) {
    os->stream_->ReadVarint32(&buffer32);
    os->stream_->ReadTag();
  }
  ow->RenderUint32(field_name, absl::bit_cast<uint32_t>(buffer32));
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderBool(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& /*type*/,
    absl::string_view field_name, ObjectWriter* ow) {
  uint32_t tag = os->stream_->ReadTag();
  uint64_t buffer64 = 0;  // results in 'false' value as default, which is the
                          // default value of Bool wrapper
  if (tag != 0) {
    os->stream_->ReadVarint64(&buffer64);
    os->stream_->ReadTag();
  }
  ow->RenderBool(field_name, buffer64 != 0);
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderString(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& /*type*/,
    absl::string_view field_name, ObjectWriter* ow) {
  uint32_t tag = os->stream_->ReadTag();
  uint32_t buffer32;
  std::string str;  // default value of empty for String wrapper
  if (tag != 0) {
    os->stream_->ReadVarint32(&buffer32);  // string size.
    os->stream_->ReadString(&str, buffer32);
    os->stream_->ReadTag();
  }
  ow->RenderString(field_name, str);
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderBytes(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& /*type*/,
    absl::string_view field_name, ObjectWriter* ow) {
  uint32_t tag = os->stream_->ReadTag();
  uint32_t buffer32;
  std::string str;
  if (tag != 0) {
    os->stream_->ReadVarint32(&buffer32);
    os->stream_->ReadString(&str, buffer32);
    os->stream_->ReadTag();
  }
  ow->RenderBytes(field_name, str);
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderStruct(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& type,
    absl::string_view field_name, ObjectWriter* ow) {
  const google::protobuf::Field* field = nullptr;
  uint32_t tag = os->stream_->ReadTag();
  ow->StartObject(field_name);
  while (tag != 0) {
    field = os->FindAndVerifyField(type, tag);
    if (field == nullptr) {
      WireFormat::SkipField(os->stream_, tag, nullptr);
      tag = os->stream_->ReadTag();
      continue;
    }
    // google.protobuf.Struct has only one field that is a map. Hence we use
    // RenderMap to render that field.
    if (os->IsMap(*field)) {
      ASSIGN_OR_RETURN(tag, os->RenderMap(field, field_name, tag, ow));
    }
  }
  ow->EndObject();
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderStructValue(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& type,
    absl::string_view field_name, ObjectWriter* ow) {
  const google::protobuf::Field* field = nullptr;
  for (uint32_t tag = os->stream_->ReadTag(); tag != 0;
       tag = os->stream_->ReadTag()) {
    field = os->FindAndVerifyField(type, tag);
    if (field == nullptr) {
      WireFormat::SkipField(os->stream_, tag, nullptr);
      continue;
    }
    RETURN_IF_ERROR(os->RenderField(field, field_name, ow));
  }
  return absl::Status();
}

// TODO(skarvaje): Avoid code duplication of for loops and SkipField logic.
absl::Status ProtoStreamObjectSource::RenderStructListValue(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& type,
    absl::string_view field_name, ObjectWriter* ow) {
  uint32_t tag = os->stream_->ReadTag();

  // Render empty list when we find empty ListValue message.
  if (tag == 0) {
    ow->StartList(field_name);
    ow->EndList();
    return absl::Status();
  }

  while (tag != 0) {
    const google::protobuf::Field* field = os->FindAndVerifyField(type, tag);
    if (field == nullptr) {
      WireFormat::SkipField(os->stream_, tag, nullptr);
      tag = os->stream_->ReadTag();
      continue;
    }
    ASSIGN_OR_RETURN(tag, os->RenderList(field, field_name, tag, ow));
  }
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderAny(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& type,
    absl::string_view field_name, ObjectWriter* ow) {
  // An Any is of the form { string type_url = 1; bytes value = 2; }
  uint32_t tag;
  std::string type_url;
  std::string value;

  // First read out the type_url and value from the proto stream
  for (tag = os->stream_->ReadTag(); tag != 0; tag = os->stream_->ReadTag()) {
    const google::protobuf::Field* field = os->FindAndVerifyField(type, tag);
    if (field == nullptr) {
      WireFormat::SkipField(os->stream_, tag, nullptr);
      continue;
    }
    // 'type_url' has field number of 1 and 'value' has field number 2
    // //google/protobuf/any.proto
    if (field->number() == 1) {
      // read type_url
      uint32_t type_url_size;
      os->stream_->ReadVarint32(&type_url_size);
      os->stream_->ReadString(&type_url, type_url_size);
    } else if (field->number() == 2) {
      // read value
      uint32_t value_size;
      os->stream_->ReadVarint32(&value_size);
      os->stream_->ReadString(&value, value_size);
    }
  }

  // If there is no value, we don't lookup the type, we just output it (if
  // present). If both type and value are empty we output an empty object.
  if (value.empty()) {
    ow->StartObject(field_name);
    if (!type_url.empty()) {
      ow->RenderString("@type", type_url);
    }
    ow->EndObject();
    return absl::Status();
  }

  // If there is a value but no type, we cannot render it, so report an error.
  if (type_url.empty()) {
    // TODO(sven): Add an external message once those are ready.
    return absl::InternalError("Invalid Any, the type_url is missing.");
  }

  absl::StatusOr<const google::protobuf::Type*> resolved_type =
      os->typeinfo_->ResolveTypeUrl(type_url);

  if (!resolved_type.ok()) {
    // Convert into an internal error, since this means the backend gave us
    // an invalid response (missing or invalid type information).
    return absl::InternalError(resolved_type.status().message());
  }
  // nested_type cannot be null at this time.
  const google::protobuf::Type* nested_type = resolved_type.value();

  io::ArrayInputStream zero_copy_stream(value.data(), value.size());
  io::CodedInputStream in_stream(&zero_copy_stream);
  // We know the type so we can render it. Recursively parse the nested stream
  // using a nested ProtoStreamObjectSource using our nested type information.
  ProtoStreamObjectSource nested_os(&in_stream, os->typeinfo_, *nested_type,
                                    os->render_options_);

  // We manually call start and end object here so we can inject the @type.
  ow->StartObject(field_name);
  ow->RenderString("@type", type_url);
  absl::Status result =
      nested_os.WriteMessage(nested_os.type_, "value", 0, false, ow);
  ow->EndObject();
  return result;
}

absl::Status ProtoStreamObjectSource::RenderFieldMask(
    const ProtoStreamObjectSource* os, const google::protobuf::Type& type,
    absl::string_view field_name, ObjectWriter* ow) {
  std::string combined;
  uint32_t buffer32;
  uint32_t paths_field_tag = 0;
  for (uint32_t tag = os->stream_->ReadTag(); tag != 0;
       tag = os->stream_->ReadTag()) {
    if (paths_field_tag == 0) {
      const google::protobuf::Field* field = os->FindAndVerifyField(type, tag);
      if (field != nullptr && field->number() == 1 &&
          field->name() == "paths") {
        paths_field_tag = tag;
      }
    }
    if (paths_field_tag != tag) {
      return absl::InternalError("Invalid FieldMask, unexpected field.");
    }
    std::string str;
    os->stream_->ReadVarint32(&buffer32);  // string size.
    os->stream_->ReadString(&str, buffer32);
    if (!combined.empty()) {
      combined.append(",");
    }
    combined.append(ConvertFieldMaskPath(str, &ToCamelCase));
  }
  ow->RenderString(field_name, combined);
  return absl::Status();
}

absl::flat_hash_map<std::string, ProtoStreamObjectSource::TypeRenderer>*
    ProtoStreamObjectSource::renderers_ = nullptr;
absl::once_flag source_renderers_init_;

void ProtoStreamObjectSource::InitRendererMap() {
  renderers_ = new absl::flat_hash_map<std::string,
                                       ProtoStreamObjectSource::TypeRenderer>();
  (*renderers_)["google.protobuf.Timestamp"] =
      &ProtoStreamObjectSource::RenderTimestamp;
  (*renderers_)["google.protobuf.Duration"] =
      &ProtoStreamObjectSource::RenderDuration;
  (*renderers_)["google.protobuf.DoubleValue"] =
      &ProtoStreamObjectSource::RenderDouble;
  (*renderers_)["google.protobuf.FloatValue"] =
      &ProtoStreamObjectSource::RenderFloat;
  (*renderers_)["google.protobuf.Int64Value"] =
      &ProtoStreamObjectSource::RenderInt64;
  (*renderers_)["google.protobuf.UInt64Value"] =
      &ProtoStreamObjectSource::RenderUInt64;
  (*renderers_)["google.protobuf.Int32Value"] =
      &ProtoStreamObjectSource::RenderInt32;
  (*renderers_)["google.protobuf.UInt32Value"] =
      &ProtoStreamObjectSource::RenderUInt32;
  (*renderers_)["google.protobuf.BoolValue"] =
      &ProtoStreamObjectSource::RenderBool;
  (*renderers_)["google.protobuf.StringValue"] =
      &ProtoStreamObjectSource::RenderString;
  (*renderers_)["google.protobuf.BytesValue"] =
      &ProtoStreamObjectSource::RenderBytes;
  (*renderers_)["google.protobuf.Any"] = &ProtoStreamObjectSource::RenderAny;
  (*renderers_)["google.protobuf.Struct"] =
      &ProtoStreamObjectSource::RenderStruct;
  (*renderers_)["google.protobuf.Value"] =
      &ProtoStreamObjectSource::RenderStructValue;
  (*renderers_)["google.protobuf.ListValue"] =
      &ProtoStreamObjectSource::RenderStructListValue;
  (*renderers_)["google.protobuf.FieldMask"] =
      &ProtoStreamObjectSource::RenderFieldMask;
  ::google::protobuf::internal::OnShutdown(&DeleteRendererMap);
}

void ProtoStreamObjectSource::DeleteRendererMap() {
  delete ProtoStreamObjectSource::renderers_;
  renderers_ = nullptr;
}

// static
ProtoStreamObjectSource::TypeRenderer*
ProtoStreamObjectSource::FindTypeRenderer(const std::string& type_url) {
  absl::call_once(source_renderers_init_, InitRendererMap);
  auto it = renderers_->find(type_url);
  if (it == renderers_->end()) return nullptr;
  return &it->second;
}

absl::Status ProtoStreamObjectSource::RenderField(
    const google::protobuf::Field* field, absl::string_view field_name,
    ObjectWriter* ow) const {
  // Short-circuit message types as it tends to call WriteMessage recursively
  // and ends up using a lot of stack space. Keep the stack usage of this
  // message small in order to preserve stack space and not crash.
  if (field->kind() == google::protobuf::Field::TYPE_MESSAGE) {
    uint32_t buffer32;
    stream_->ReadVarint32(&buffer32);  // message length
    int old_limit = stream_->PushLimit(buffer32);
    // Get the nested message type for this field.
    const google::protobuf::Type* type =
        typeinfo_->GetTypeByTypeUrl(field->type_url());
    if (type == nullptr) {
      return absl::InternalError(
          absl::StrCat("Invalid configuration. Could not find the type: ",
                       field->type_url()));
    }

    // Short-circuit any special type rendering to save call-stack space.
    const TypeRenderer* type_renderer = FindTypeRenderer(type->name());

    RETURN_IF_ERROR(IncrementRecursionDepth(type->name(), field_name));
    if (type_renderer != nullptr) {
      RETURN_IF_ERROR((*type_renderer)(this, *type, field_name, ow));
    } else {
      RETURN_IF_ERROR(WriteMessage(*type, field_name, 0, true, ow));
    }
    --recursion_depth_;

    if (!stream_->ConsumedEntireMessage()) {
      return absl::InvalidArgumentError(
          "Nested protocol message not parsed in its entirety.");
    }
    stream_->PopLimit(old_limit);
  } else {
    // Render all other non-message types.
    return RenderNonMessageField(field, field_name, ow);
  }
  return absl::Status();
}

absl::Status ProtoStreamObjectSource::RenderNonMessageField(
    const google::protobuf::Field* field, absl::string_view field_name,
    ObjectWriter* ow) const {
  // Temporary buffers of different types.
  uint32_t buffer32 = 0;
  uint64_t buffer64 = 0;
  std::string strbuffer;
  switch (field->kind()) {
    case google::protobuf::Field::TYPE_BOOL: {
      stream_->ReadVarint64(&buffer64);
      ow->RenderBool(field_name, buffer64 != 0);
      break;
    }
    case google::protobuf::Field::TYPE_INT32: {
      stream_->ReadVarint32(&buffer32);
      ow->RenderInt32(field_name, absl::bit_cast<int32_t>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_INT64: {
      stream_->ReadVarint64(&buffer64);
      ow->RenderInt64(field_name, absl::bit_cast<int64_t>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_UINT32: {
      stream_->ReadVarint32(&buffer32);
      ow->RenderUint32(field_name, absl::bit_cast<uint32_t>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_UINT64: {
      stream_->ReadVarint64(&buffer64);
      ow->RenderUint64(field_name, absl::bit_cast<uint64_t>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_SINT32: {
      stream_->ReadVarint32(&buffer32);
      ow->RenderInt32(field_name, WireFormatLite::ZigZagDecode32(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_SINT64: {
      stream_->ReadVarint64(&buffer64);
      ow->RenderInt64(field_name, WireFormatLite::ZigZagDecode64(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_SFIXED32: {
      stream_->ReadLittleEndian32(&buffer32);
      ow->RenderInt32(field_name, absl::bit_cast<int32_t>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_SFIXED64: {
      stream_->ReadLittleEndian64(&buffer64);
      ow->RenderInt64(field_name, absl::bit_cast<int64_t>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_FIXED32: {
      stream_->ReadLittleEndian32(&buffer32);
      ow->RenderUint32(field_name, absl::bit_cast<uint32_t>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_FIXED64: {
      stream_->ReadLittleEndian64(&buffer64);
      ow->RenderUint64(field_name, absl::bit_cast<uint64_t>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_FLOAT: {
      stream_->ReadLittleEndian32(&buffer32);
      ow->RenderFloat(field_name, absl::bit_cast<float>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_DOUBLE: {
      stream_->ReadLittleEndian64(&buffer64);
      ow->RenderDouble(field_name, absl::bit_cast<double>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_ENUM: {
      stream_->ReadVarint32(&buffer32);

      // If the field represents an explicit NULL value, render null.
      if (field->type_url() == kStructNullValueTypeUrl) {
        ow->RenderNull(field_name);
        break;
      }

      // Get the nested enum type for this field.
      // TODO(skarvaje): Avoid string manipulation. Find ways to speed this
      // up.
      const google::protobuf::Enum* en =
          typeinfo_->GetEnumByTypeUrl(field->type_url());
      // Lookup the name of the enum, and render that. Unknown enum values
      // are printed as integers.
      if (en != nullptr) {
        const google::protobuf::EnumValue* enum_value =
            FindEnumValueByNumber(*en, buffer32);
        if (enum_value != nullptr) {
          if (render_options_.use_ints_for_enums) {
            ow->RenderInt32(field_name, buffer32);
          } else if (render_options_.use_lower_camel_for_enums) {
            ow->RenderString(field_name,
                             EnumValueNameToLowerCamelCase(enum_value->name()));
          } else {
            ow->RenderString(field_name, enum_value->name());
          }
        } else {
          ow->RenderInt32(field_name, buffer32);
        }
      } else {
        ow->RenderInt32(field_name, buffer32);
      }
      break;
    }
    case google::protobuf::Field::TYPE_STRING: {
      stream_->ReadVarint32(&buffer32);  // string size.
      stream_->ReadString(&strbuffer, buffer32);
      ow->RenderString(field_name, strbuffer);
      break;
    }
    case google::protobuf::Field::TYPE_BYTES: {
      stream_->ReadVarint32(&buffer32);  // bytes size.
      stream_->ReadString(&strbuffer, buffer32);
      ow->RenderBytes(field_name, strbuffer);
      break;
    }
    default:
      break;
  }
  return absl::Status();
}

// TODO(skarvaje): Fix this to avoid code duplication.
std::string ProtoStreamObjectSource::ReadFieldValueAsString(
    const google::protobuf::Field& field) const {
  std::string result;
  switch (field.kind()) {
    case google::protobuf::Field::TYPE_BOOL: {
      uint64_t buffer64;
      stream_->ReadVarint64(&buffer64);
      result = buffer64 != 0 ? "true" : "false";
      break;
    }
    case google::protobuf::Field::TYPE_INT32: {
      uint32_t buffer32;
      stream_->ReadVarint32(&buffer32);
      result = absl::StrCat(absl::bit_cast<int32_t>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_INT64: {
      uint64_t buffer64;
      stream_->ReadVarint64(&buffer64);
      result = absl::StrCat(absl::bit_cast<int64_t>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_UINT32: {
      uint32_t buffer32;
      stream_->ReadVarint32(&buffer32);
      result = absl::StrCat(absl::bit_cast<uint32_t>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_UINT64: {
      uint64_t buffer64;
      stream_->ReadVarint64(&buffer64);
      result = absl::StrCat(absl::bit_cast<uint64_t>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_SINT32: {
      uint32_t buffer32;
      stream_->ReadVarint32(&buffer32);
      result = absl::StrCat(WireFormatLite::ZigZagDecode32(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_SINT64: {
      uint64_t buffer64;
      stream_->ReadVarint64(&buffer64);
      result = absl::StrCat(WireFormatLite::ZigZagDecode64(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_SFIXED32: {
      uint32_t buffer32;
      stream_->ReadLittleEndian32(&buffer32);
      result = absl::StrCat(absl::bit_cast<int32_t>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_SFIXED64: {
      uint64_t buffer64;
      stream_->ReadLittleEndian64(&buffer64);
      result = absl::StrCat(absl::bit_cast<int64_t>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_FIXED32: {
      uint32_t buffer32;
      stream_->ReadLittleEndian32(&buffer32);
      result = absl::StrCat(absl::bit_cast<uint32_t>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_FIXED64: {
      uint64_t buffer64;
      stream_->ReadLittleEndian64(&buffer64);
      result = absl::StrCat(absl::bit_cast<uint64_t>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_FLOAT: {
      uint32_t buffer32;
      stream_->ReadLittleEndian32(&buffer32);
      result = SimpleFtoa(absl::bit_cast<float>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_DOUBLE: {
      uint64_t buffer64;
      stream_->ReadLittleEndian64(&buffer64);
      result = SimpleDtoa(absl::bit_cast<double>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_ENUM: {
      uint32_t buffer32;
      stream_->ReadVarint32(&buffer32);
      // Get the nested enum type for this field.
      // TODO(skarvaje): Avoid string manipulation. Find ways to speed this
      // up.
      const google::protobuf::Enum* en =
          typeinfo_->GetEnumByTypeUrl(field.type_url());
      // Lookup the name of the enum, and render that. Skips unknown enums.
      if (en != nullptr) {
        const google::protobuf::EnumValue* enum_value =
            FindEnumValueByNumber(*en, buffer32);
        if (enum_value != nullptr) {
          result = enum_value->name();
        }
      }
      break;
    }
    case google::protobuf::Field::TYPE_STRING: {
      uint32_t buffer32;
      stream_->ReadVarint32(&buffer32);  // string size.
      stream_->ReadString(&result, buffer32);
      break;
    }
    case google::protobuf::Field::TYPE_BYTES: {
      uint32_t buffer32;
      stream_->ReadVarint32(&buffer32);  // bytes size.
      stream_->ReadString(&result, buffer32);
      break;
    }
    default:
      break;
  }
  return result;
}

// Field is a map if it is a repeated message and it has an option "map_type".
// TODO(skarvaje): Consider pre-computing the IsMap() into Field directly.
bool ProtoStreamObjectSource::IsMap(
    const google::protobuf::Field& field) const {
  const google::protobuf::Type* field_type =
      typeinfo_->GetTypeByTypeUrl(field.type_url());
  return field.kind() == google::protobuf::Field::TYPE_MESSAGE &&
         util::converter::IsMap(field, *field_type);
}

std::pair<int64_t, int32_t> ProtoStreamObjectSource::ReadSecondsAndNanos(
    const google::protobuf::Type& type) const {
  uint64_t seconds = 0;
  uint32_t nanos = 0;
  uint32_t tag = 0;
  int64_t signed_seconds = 0;
  int32_t signed_nanos = 0;

  for (tag = stream_->ReadTag(); tag != 0; tag = stream_->ReadTag()) {
    const google::protobuf::Field* field = FindAndVerifyField(type, tag);
    if (field == nullptr) {
      WireFormat::SkipField(stream_, tag, nullptr);
      continue;
    }
    // 'seconds' has field number of 1 and 'nanos' has field number 2
    // //google/protobuf/timestamp.proto & duration.proto
    if (field->number() == 1) {
      // read seconds
      stream_->ReadVarint64(&seconds);
      signed_seconds = absl::bit_cast<int64_t>(seconds);
    } else if (field->number() == 2) {
      // read nanos
      stream_->ReadVarint32(&nanos);
      signed_nanos = absl::bit_cast<int32_t>(nanos);
    }
  }
  return std::pair<int64_t, int32_t>(signed_seconds, signed_nanos);
}

absl::Status ProtoStreamObjectSource::IncrementRecursionDepth(
    absl::string_view type_name, absl::string_view field_name) const {
  if (++recursion_depth_ > max_recursion_depth_) {
    return absl::InvalidArgumentError(
        absl::StrCat("Message too deep. Max recursion depth reached for type '",
                     type_name, "', field '", field_name, "'"));
  }
  return absl::Status();
}

namespace {
// TODO(skarvaje): Speed this up by not doing a linear scan.
const google::protobuf::Field* FindFieldByNumber(
    const google::protobuf::Type& type, int number) {
  for (int i = 0; i < type.fields_size(); ++i) {
    if (type.fields(i).number() == number) {
      return &type.fields(i);
    }
  }
  return nullptr;
}

// TODO(skarvaje): Replace FieldDescriptor by implementing IsTypePackable()
// using tech Field.
bool IsPackable(const google::protobuf::Field& field) {
  return field.cardinality() == google::protobuf::Field::CARDINALITY_REPEATED &&
         FieldDescriptor::IsTypePackable(
             static_cast<FieldDescriptor::Type>(field.kind()));
}

// TODO(skarvaje): Speed this up by not doing a linear scan.
const google::protobuf::EnumValue* FindEnumValueByNumber(
    const google::protobuf::Enum& tech_enum, int number) {
  for (int i = 0; i < tech_enum.enumvalue_size(); ++i) {
    const google::protobuf::EnumValue& ev = tech_enum.enumvalue(i);
    if (ev.number() == number) {
      return &ev;
    }
  }
  return nullptr;
}

// TODO(skarvaje): Look into optimizing this by not doing computation on
// double.
std::string FormatNanos(uint32_t nanos, bool with_trailing_zeros) {
  if (nanos == 0) {
    return with_trailing_zeros ? ".000" : "";
  }

  const int precision = (nanos % 1000 != 0)      ? 9
                        : (nanos % 1000000 != 0) ? 6
                                                 : 3;
  std::string formatted = absl::StrFormat(
      "%.*f", precision, static_cast<double>(nanos) / kNanosPerSecond);
  // remove the leading 0 before decimal.
  return formatted.substr(1);
}
}  // namespace

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
