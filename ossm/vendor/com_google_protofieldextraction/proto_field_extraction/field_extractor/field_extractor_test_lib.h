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

#ifndef PROTO_FIELD_EXTRACTION_SRC_FIELD_EXTRACTOR_FIELD_EXTRACTOR_TEST_LIB_H_
#define PROTO_FIELD_EXTRACTION_SRC_FIELD_EXTRACTOR_FIELD_EXTRACTOR_TEST_LIB_H_

#include <functional>
#include <string>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/type.pb.h"
#include "absl/status/statusor.h"
#include "proto_field_extraction/test_utils/testdata/field_extractor_test.pb.h"
#include "google/protobuf/io/coded_stream.h"

namespace google::protobuf::field_extraction::testing {

using ::google::protobuf::Any;
using ::google::protobuf::Field;
using ::google::protobuf::Type;
using ::google::protobuf::io::CodedInputStream;

// Top level of the message type url.
constexpr char kFieldExtractorTestMessageTypeUrl[] =
    "type.googleapis.com/"
    "google.protobuf.field_extraction.testing.FieldExtractorTestMessage";

// Declares the functors to extract various field types used by the field
// extractor under tests.
//
// Definitions are placed at the end of the file to avoid sinking the actual
// test cases.
template <typename T>
using FieldInfoExtractorFunc = std::function<absl::StatusOr<T>(
    const Type&, const Field*, CodedInputStream*)>;
template <typename T>
using FieldInfoMapExtractorFunc = std::function<absl::StatusOr<T>(
    const Field*, const Field*, const Field*, CodedInputStream*)>;

FieldInfoExtractorFunc<std::string> GetDummyStringFieldExtractor();
FieldInfoExtractorFunc<std::string> GetStringFieldExtractor();
FieldInfoExtractorFunc<TestEnum> GetTestEnumFieldExtractor();
FieldInfoExtractorFunc<bool> GetBoolFieldExtractor();
FieldInfoExtractorFunc<double> GetDoubleFieldExtractor();
FieldInfoExtractorFunc<float> GetFloatFieldExtractor();
FieldInfoExtractorFunc<int64_t> GetInt64FieldExtractor();
FieldInfoExtractorFunc<uint64_t> GetUInt64FieldExtractor();
FieldInfoExtractorFunc<int> GetInt32FieldExtractor();
FieldInfoExtractorFunc<uint64_t> GetFixed64FieldExtractor();
FieldInfoExtractorFunc<uint32_t> GetFixed32FieldExtractor();
FieldInfoExtractorFunc<uint32_t> GetUInt32FieldExtractor();
FieldInfoExtractorFunc<int64_t> GetSFixed64FieldExtractor();
FieldInfoExtractorFunc<int32_t> GetSFixed32FieldExtractor();
FieldInfoExtractorFunc<int32_t> GetSInt32FieldExtractor();
FieldInfoExtractorFunc<int64_t> GetSInt64FieldExtractor();
FieldInfoExtractorFunc<SingularFieldTestMessage>
GetSingularMessageFieldExtractor();
FieldInfoExtractorFunc<std::vector<SingularFieldTestMessage>>
GetRepeatedMessageFieldExtractor();
FieldInfoExtractorFunc<std::vector<std::string>>
GetRepeatedStringFieldExtractor();
FieldInfoMapExtractorFunc<std::vector<std::string>>
GetRepeatedStringMapFieldExtractor();
FieldInfoExtractorFunc<std::vector<int32_t>> GetRepeatedInt32FieldExtractor();
FieldInfoExtractorFunc<int64_t> GetFieldInfoCountingExtractor();
FieldInfoExtractorFunc<Any> GetAnyFieldExtractor();

}  // namespace google::protobuf::field_extraction::testing

#endif  // PROTO_FIELD_EXTRACTION_SRC_FIELD_EXTRACTOR_FIELD_EXTRACTOR_TEST_LIB_H_
