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

#ifndef PROTO_FIELD_EXTRACTION_SRC_TEST_UTILS_UTILS_H_
#define PROTO_FIELD_EXTRACTION_SRC_TEST_UTILS_UTILS_H_

#include "google/protobuf/descriptor.pb.h"
#include "absl/status/statusor.h"
#include "grpc_transcoding/type_helper.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/coded_stream.h"

namespace google::protobuf::field_extraction::testing {

absl::Status GetContents(absl::string_view file_name, std::string* output);

absl::StatusOr<google::protobuf::FileDescriptorSet> GetDescriptorFromBinary(
    absl::string_view filename);

absl::Status GetTextProto(absl::string_view filename, google::protobuf::Message* proto);

std::string GetTestDataFilePath(absl::string_view path);

class TypeHelper {
 public:
  absl::StatusOr<std::unique_ptr<TypeHelper>> static Create(
      absl::string_view descriptor_path);

  const google::protobuf::Type* ResolveTypeUrl(
      absl::string_view type_url) const;

 private:
  std::unique_ptr<google::grpc::transcoding::TypeHelper> type_helper_;
  google::protobuf::DescriptorPool descriptor_pool_;
};

}  // namespace google::protobuf::field_extraction::testing

#endif  // PROTO_FIELD_EXTRACTION_SRC_TEST_UTILS_UTILS_H_
