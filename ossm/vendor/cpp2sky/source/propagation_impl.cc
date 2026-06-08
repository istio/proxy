// Copyright 2020 SkyAPM

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "source/propagation_impl.h"

#include <array>
#include <memory>

#include "absl/memory/memory.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "cpp2sky/exception.h"
#include "source/utils/base64.h"

namespace cpp2sky {

namespace {
static constexpr size_t EXPECTED_FIELD_COUNT = 8;

// TODO(shikugawa): This value specifies the number of values on `sw8-x` header.
// This value should be extensible from user config to deliver arbitary
// information as SpanContext.
static constexpr size_t EXPECTED_EXTENSION_FIELD_COUNT = 1;
}  // namespace

SpanContextImpl::SpanContextImpl(absl::string_view header_value) {
  std::array<std::string, EXPECTED_FIELD_COUNT> fields;
  size_t current_field_idx = 0;
  std::string value;

  for (size_t i = 0; i < header_value.size(); ++i) {
    if (current_field_idx >= EXPECTED_FIELD_COUNT) {
      throw TracerException(
          "Invalid span context format. It must have 8 fields.");
    }
    if (header_value[i] == '-') {
      fields[current_field_idx] = value;
      value.clear();
      ++current_field_idx;
      continue;
    }
    value += header_value[i];
  }
  fields[current_field_idx] = value;

  if (current_field_idx != EXPECTED_FIELD_COUNT - 1) {
    throw TracerException(
        "Invalid span context format. It must have 8 fields.");
  }

  if (fields[0] != "0" && fields[0] != "1") {
    throw TracerException(
        "Invalid span context format. Sample field must be 0 or 1.");
  }

  // Sampling is always true
  sample_ = true;
  trace_id_ = Base64::decodeWithoutPadding(absl::string_view(fields[1]));
  trace_segment_id_ =
      Base64::decodeWithoutPadding(absl::string_view(fields[2]));

  if (!absl::SimpleAtoi(fields[3], &span_id_)) {
    throw TracerException(
        "Invalid span id format. Span id field must be integer number.");
  }

  service_ = Base64::decodeWithoutPadding(absl::string_view(fields[4]));
  service_instance_ =
      Base64::decodeWithoutPadding(absl::string_view(fields[5]));
  endpoint_ = Base64::decodeWithoutPadding(absl::string_view(fields[6]));
  target_address_ = Base64::decodeWithoutPadding(absl::string_view(fields[7]));
}

SpanContextExtensionImpl::SpanContextExtensionImpl(
    absl::string_view header_value) {
  std::array<std::string, EXPECTED_EXTENSION_FIELD_COUNT> fields;
  size_t current_field_idx = 0;
  std::string value;

  for (size_t i = 0; i < header_value.size(); ++i) {
    if (current_field_idx >= EXPECTED_EXTENSION_FIELD_COUNT) {
      throw TracerException(
          "Invalid span context format. It must have 1 fields.");
    }
    if (header_value[i] == '-') {
      fields[current_field_idx] = value;
      value.clear();
      ++current_field_idx;
      continue;
    }
    value += header_value[i];
  }
  fields[current_field_idx] = value;

  if (current_field_idx != EXPECTED_EXTENSION_FIELD_COUNT - 1) {
    throw TracerException(
        "Invalid span context format. It must have 1 fields.");
  }

  if (fields[0] != "0" && fields[0] != "1") {
    throw TracerException(
        "Invalid span context format. tracing mode field must be 0 or 1.");
  }

  if (fields[0] == "1") {
    tracing_mode_ = TracingMode::Skip;
  }
}

SpanContextSharedPtr createSpanContext(absl::string_view ctx) {
  return std::make_shared<SpanContextImpl>(ctx);
}

SpanContextExtensionSharedPtr createSpanContextExtension(
    absl::string_view ctx) {
  return std::make_shared<SpanContextExtensionImpl>(ctx);
}

}  // namespace cpp2sky
