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

#include "google/protobuf/util/converter/json_objectwriter.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "google/protobuf/stubs/strutil.h"
#include "absl/base/casts.h"
#include "absl/log/absl_log.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/util/converter/json_escaping.h"
#include "google/protobuf/util/converter/utility.h"


namespace google {
namespace protobuf {
namespace util {
namespace converter {

JsonObjectWriter::~JsonObjectWriter() {
  if (element_ && !element_->is_root()) {
    ABSL_LOG(WARNING) << "JsonObjectWriter was not fully closed.";
  }
}

JsonObjectWriter* JsonObjectWriter::StartObject(absl::string_view name) {
  WritePrefix(name);
  WriteChar('{');
  PushObject();
  return this;
}

JsonObjectWriter* JsonObjectWriter::EndObject() {
  Pop();
  WriteChar('}');
  if (element() && element()->is_root()) NewLine();
  return this;
}

JsonObjectWriter* JsonObjectWriter::StartList(absl::string_view name) {
  WritePrefix(name);
  WriteChar('[');
  PushArray();
  return this;
}

JsonObjectWriter* JsonObjectWriter::EndList() {
  Pop();
  WriteChar(']');
  if (element()->is_root()) NewLine();
  return this;
}

JsonObjectWriter* JsonObjectWriter::RenderBool(absl::string_view name,
                                               bool value) {
  return RenderSimple(name, value ? "true" : "false");
}

JsonObjectWriter* JsonObjectWriter::RenderInt32(absl::string_view name,
                                                int32_t value) {
  return RenderSimple(name, absl::StrCat(value));
}

JsonObjectWriter* JsonObjectWriter::RenderUint32(absl::string_view name,
                                                 uint32_t value) {
  return RenderSimple(name, absl::StrCat(value));
}

JsonObjectWriter* JsonObjectWriter::RenderInt64(absl::string_view name,
                                                int64_t value) {
  WritePrefix(name);
  WriteChar('"');
  WriteRawString(absl::StrCat(value));
  WriteChar('"');
  return this;
}

JsonObjectWriter* JsonObjectWriter::RenderUint64(absl::string_view name,
                                                 uint64_t value) {
  WritePrefix(name);
  WriteChar('"');
  WriteRawString(absl::StrCat(value));
  WriteChar('"');
  return this;
}

JsonObjectWriter* JsonObjectWriter::RenderDouble(absl::string_view name,
                                                 double value) {
  if (std::isfinite(value)) {
    return RenderSimple(name, SimpleDtoa(value));
  }

  // Render quoted with NaN/Infinity-aware DoubleAsString.
  return RenderString(name, DoubleAsString(value));
}

JsonObjectWriter* JsonObjectWriter::RenderFloat(absl::string_view name,
                                                float value) {
  if (std::isfinite(value)) {
    return RenderSimple(name, SimpleFtoa(value));
  }

  // Render quoted with NaN/Infinity-aware FloatAsString.
  return RenderString(name, FloatAsString(value));
}

JsonObjectWriter* JsonObjectWriter::RenderString(absl::string_view name,
                                                 absl::string_view value) {
  WritePrefix(name);
  WriteChar('"');
  JsonEscaping::Escape(value, &sink_);
  WriteChar('"');
  return this;
}

JsonObjectWriter* JsonObjectWriter::RenderBytes(absl::string_view name,
                                                absl::string_view value) {
  WritePrefix(name);
  std::string base64;

  if (use_websafe_base64_for_bytes_)
    WebSafeBase64EscapeWithPadding(value, &base64);
  else
    absl::Base64Escape(value, &base64);

  WriteChar('"');
  // TODO(wpoon): Consider a ByteSink solution that writes the base64 bytes
  //              directly to the stream, rather than first putting them
  //              into a string and then writing them to the stream.
  stream_->WriteRaw(base64.data(), base64.size());
  WriteChar('"');
  return this;
}

JsonObjectWriter* JsonObjectWriter::RenderNull(absl::string_view name) {
  return RenderSimple(name, "null");
}

JsonObjectWriter* JsonObjectWriter::RenderNullAsEmpty(absl::string_view name) {
  return RenderSimple(name, "");
}

void JsonObjectWriter::WritePrefix(absl::string_view name) {
  bool not_first = !element()->is_first();
  if (not_first) WriteChar(',');
  if (not_first || !element()->is_root()) NewLine();
  if (!name.empty() || element()->is_json_object()) {
    WriteChar('"');
    if (!name.empty()) {
      JsonEscaping::Escape(name, &sink_);
    }
    WriteRawString("\":");
    if (!indent_string_.empty()) WriteChar(' ');
  }
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
