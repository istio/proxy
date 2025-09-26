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

#ifndef PROTO_FIELD_EXTRACTION_SRC_FIELD_VALUE_EXTRACTOR_FIELD_VALUE_EXTRACTOR_H_
#define PROTO_FIELD_EXTRACTION_SRC_FIELD_VALUE_EXTRACTOR_FIELD_VALUE_EXTRACTOR_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/type.pb.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/field_extractor/field_extractor.h"
#include "proto_field_extraction/field_value_extractor/field_value_extractor_interface.h"

namespace google::protobuf::field_extraction {

using CreateFieldExtractorFunc =
    std::function<std::unique_ptr<FieldExtractor>()>;

// FieldValueExtractor extracts field value(s) for a given field path
// within a given message type.
class FieldValueExtractor : public FieldValueExtractorInterface {
 public:
  // DO *NOT* Call this constructor directly.
  // Use `FieldValueExtractorFactory::Create()` instead.
  explicit FieldValueExtractor(
      absl::string_view field_path,
      CreateFieldExtractorFunc create_field_extrator_func)
      : field_path_(field_path),
        field_extractor_(create_field_extrator_func()) {}

  FieldValueExtractor(const FieldValueExtractor&) = delete;

  FieldValueExtractor& operator=(const FieldValueExtractor&) = delete;

  // Extracts the proto field value(s) that match the field path within root
  // message type from the proto `message`.
  // If any of the field path nodes is a repeated field (including map)
  // containing multiple values, then the return vector will contain multiple
  // strings.
  //
  // Returns an error status if the given proto `message` type does not
  // match the type specified by the root message type that was used to create
  // a field extractor. All the other error status should be caught and return
  // in `FieldValueExtractorFactory::Create()`.
  absl::StatusOr<std::vector<std::string>> Extract(
      const CodedInputStreamWrapperFactory& message) const override;

  // Extracts the proto field value(s) that match the field path within root
  // message type from the proto `message` and extracted values(s) are contained
  // in `list_value` of the returned `Value`. If any of the field path nodes is
  // a repeated field (including map) containing multiple values, the
  // returned `list_value` will contain multiple elements.
  //
  // Returns an error status if the given proto `message` type does not
  // match the type specified by the root message type that was used to create
  // a field extractor. All the other error status should be caught and return
  // in `FieldValueExtractorFactory::Create()`.
  absl::StatusOr<Value> ExtractValue(
      const CodedInputStreamWrapperFactory& message) const override;

 private:
  // Field path.
  const std::string field_path_;

  std::unique_ptr<FieldExtractor> field_extractor_;
};

}  // namespace google::protobuf::field_extraction

#endif  // PROTO_FIELD_EXTRACTION_SRC_FIELD_VALUE_EXTRACTOR_FIELD_VALUE_EXTRACTOR_H_
