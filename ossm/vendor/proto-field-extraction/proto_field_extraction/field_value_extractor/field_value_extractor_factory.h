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

#ifndef PROTO_FIELD_EXTRACTION_SRC_STRING_IN_FIELD_EXTRACTOR_STRING_IN_FIELD_EXTRACTOR_FACTORY_H_
#define PROTO_FIELD_EXTRACTION_SRC_STRING_IN_FIELD_EXTRACTOR_STRING_IN_FIELD_EXTRACTOR_FACTORY_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "google/api/service.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "proto_field_extraction/field_extractor/field_extractor.h"
#include "proto_field_extraction/field_value_extractor/field_value_extractor_interface.h"

namespace google::protobuf::field_extraction {

struct FieldMetadata {
  // JSON name of the proto field path. eg. `user.displayName`.
  const std::string field_path_json_name;
};

// FieldValueExtractorFactory creates FieldValueExtractors for
// extracting field value(s) for a given field path within a given message type.
class FieldValueExtractorFactory : public FieldValueExtractorInterfaceFactory {
 public:
  explicit FieldValueExtractorFactory(
      google::protobuf::field_extraction::TypeFindFunc type_finder)
      : type_finder_(std::move(type_finder)) {}

  FieldValueExtractorFactory(const FieldValueExtractorFactory&) = delete;

  // Returns a FieldExtractor after performing `ValidateFieldPath()` to detect
  // whether if the invalid field paths in the given root message type is given.
  // An error status is returned if any of the validation check fails.
  absl::StatusOr<std::unique_ptr<FieldValueExtractorInterface>> Create(
      absl::string_view message_type,
      absl::string_view field_path) const override;

  // Returns a FieldExtractor after performing `ValidateFieldPath()` to detect
  // whether if the invalid field paths in the given root message type is given.
  // An error status is returned if any of the validation check fails.
  // When 'support_any" is true, field with a protobuf.Any type will stop
  // validating rest of the field path because there is no enough information to
  // perform validation at configuration time. Instead, protobuf.Any field will
  // validate at runtime based on the type information.
  absl::StatusOr<std::unique_ptr<FieldValueExtractorInterface>> Create(
      absl::string_view message_type, absl::string_view field_path,
      bool support_any, absl::string_view custom_proto_map_entry_name) const;

  FieldValueExtractorFactory& operator=(const FieldValueExtractorFactory&) =
      delete;

  // Validates the given `field_path` in the given root `message_type` and
  // returns the `FieldMetadata` if valid.
  //
  // The validation covers:
  // 1. Each field name in `field_path` must match one proto field within given
  // `message_type`.
  // 2. All non-leaf nodes must be of a message or a map type.
  // 3. The leaf node that matches `field_path` must be either a repeated or a
  //    single numeric or string type. (it cannot be a map field or other types
  //    like enum or bytes or bool).
  //
  // when 'support_any' is true, validation is stopped at the protobuf.Any node
  // because we do not have enough information about the real type at the
  // configuration. `field_path` will be returned as-it-is because the JSON name
  // cannot be determined.
  //
  // An example for the `field_path` is "foo.bar.baz". The message type
  // must contain a message type field "foo", which in turn must contain a
  // message type field "bar", which in turn must contain a numeric or string
  // type field "baz".
  // More details, see go/proto-encoding.
  static absl::StatusOr<FieldMetadata> ValidateFieldPathAndCollectMetadata(
      absl::string_view message_type, absl::string_view field_path,
      bool support_any,
      google::protobuf::field_extraction::TypeFindFunc type_finder,
      absl::string_view custom_proto_map_entry_name = "");

 private:
  // Looks up types for the method based on type URL.
  std::function<const google::protobuf::Type*(const std::string&)> type_finder_;
};

}  // namespace google::protobuf::field_extraction

#endif  // PROTO_FIELD_EXTRACTION_SRC_STRING_IN_FIELD_EXTRACTOR_STRING_IN_FIELD_EXTRACTOR_FACTORY_H_
