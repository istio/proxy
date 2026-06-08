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

#ifndef PROTO_FIELD_EXTRACTION_SRC_UTILS_CONSTANTS_H_
#define PROTO_FIELD_EXTRACTION_SRC_UTILS_CONSTANTS_H_

namespace google::protobuf::field_extraction {

// Type string for google.protobuf.Any
extern const char* const kAnyType;

// The constants for proto map.
extern const char* const kProtoMapEntryName;
extern const char* const kProtoMapKeyFieldName;
extern const char* const kProtoMapValueFieldName;

}  // namespace google::protobuf::field_extraction

#endif  // PROTO_FIELD_EXTRACTION_SRC_UTILS_CONSTANTS_H_
