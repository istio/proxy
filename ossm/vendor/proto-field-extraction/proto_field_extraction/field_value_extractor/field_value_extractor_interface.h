/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PROTO_FIELD_EXTRACTION_SRC_FIELD_VALUE_EXTRACTOR_FIELD_VALUE_EXTRACTOR_INTERFACE_H_
#define PROTO_FIELD_EXTRACTION_SRC_FIELD_VALUE_EXTRACTOR_FIELD_VALUE_EXTRACTOR_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/message_data/message_data.h"

namespace google::protobuf::field_extraction {

// Interface for field extractors that extract field values in string
// representation from a proto message.
//
// Note: Each FieldValueExtractorInterface instance should handle a
// specific field in a specific message type.
class FieldValueExtractorInterface {
 public:
  virtual ~FieldValueExtractorInterface() = default;

  // Extracts the proto field value(s) from a proto `message`.
  virtual absl::StatusOr<std::vector<std::string>> Extract(
      const CodedInputStreamWrapperFactory& message) const = 0;

  // Extracts the proto field as `google.protobuf.Value` from a proto `message`.
  virtual absl::StatusOr<Value> ExtractValue(
      const CodedInputStreamWrapperFactory& message) const = 0;
};

// Interface for field extractor factory.
class FieldValueExtractorInterfaceFactory {
 public:
  virtual ~FieldValueExtractorInterfaceFactory() = default;

  // Creates field extractor using the given `message_type` and `field_path`.
  // `message_type` has the format of a type URL,
  // i.e. type.googleapis.com/pkg.to.Message
  virtual absl::StatusOr<std::unique_ptr<FieldValueExtractorInterface>> Create(
      absl::string_view message_type, absl::string_view field_path) const = 0;
};

}  // namespace google::protobuf::field_extraction

#endif  // PROTO_FIELD_EXTRACTION_SRC_FIELD_VALUE_EXTRACTOR_FIELD_VALUE_EXTRACTOR_INTERFACE_H_
