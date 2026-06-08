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

#include "proto_field_extraction/test_utils/utils.h"

#include <memory>

#include "google/protobuf/descriptor.pb.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/type_resolver_util.h"

namespace google::protobuf::field_extraction::testing {

using ::google::protobuf::Type;

absl::Status GetContents(absl::string_view file_name, std::string* output) {
  FILE* fp = fopen(file_name.data(), "rb");
  if (fp == NULL) {
    return absl::InvalidArgumentError(
        absl::StrCat("Can't find file: ", file_name));
  }

  output->clear();
  while (!feof(fp)) {
    char buf[4096];
    size_t ret = fread(buf, 1, 4096, fp);
    if (ret == 0 && ferror(fp)) {
      return absl::InternalError(
          absl::StrCat("Error while reading file: ", file_name));
    }
    output->append(std::string(buf, ret));
  }
  fclose(fp);
  return absl::OkStatus();
}

absl::StatusOr<google::protobuf::FileDescriptorSet> GetDescriptorFromBinary(
    absl::string_view filename) {
  std::string content;
  auto status = GetContents(filename, &content);
  if (!status.ok()) return status;

  google::protobuf::FileDescriptorSet descriptor_set;
  if (!descriptor_set.ParseFromString(content)) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        absl::StrCat("Could not parse file contents of ",
                                     filename, " as wire-format protobuf"));
  }
  if (!descriptor_set.IsInitialized()) {
    return absl::Status(
        absl::StatusCode::kFailedPrecondition,
        absl::StrCat("Could not parse file contents of ", filename,
                     ", result uninitialized: ",
                     descriptor_set.InitializationErrorString()));
  }
  return descriptor_set;
}

std::string GetTestDataFilePath(absl::string_view path) {
  return absl::StrCat("proto_field_extraction/", std::string(path));
}

absl::Status GetTextProto(absl::string_view filename,
                          ::google::protobuf::Message* proto) {
  std::string proto_str;
  auto status = GetContents(filename, &proto_str);
  if (!status.ok()) return status;

  if (!::google::protobuf::TextFormat::ParseFromString(proto_str, proto)) {
    return absl::Status(
        absl::StatusCode::kFailedPrecondition,
        absl::StrCat("Could not parse file contents of ", filename,
                     " as text fromat protobuf of type ",
                     proto->GetTypeName()));
  }
  if (!proto->IsInitialized()) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        absl::StrCat("Could not parse file contents of ",
                                     filename, ", result uninitialized: ",
                                     proto->InitializationErrorString()));
  }

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<TypeHelper>> TypeHelper::Create(
    absl::string_view descriptor_path) {
  auto descriptor_set = GetDescriptorFromBinary(descriptor_path);
  if (!descriptor_set.ok()) return descriptor_set.status();

  auto type_helper = std::make_unique<TypeHelper>();
  for (const auto& file : descriptor_set->file()) {
    type_helper->descriptor_pool_.BuildFile(file);
  }
  type_helper->type_helper_ =
      std::make_unique<google::grpc::transcoding::TypeHelper>(
          google::protobuf::util::NewTypeResolverForDescriptorPool(
              "type.googleapis.com", &type_helper->descriptor_pool_));
  return std::move(type_helper);
}

const Type* TypeHelper::ResolveTypeUrl(absl::string_view type_url) const {
  ABSL_CHECK(type_helper_ != nullptr);
  auto result = type_helper_->Info()->ResolveTypeUrl(type_url.data());
  return result.ok() ? result.value() : nullptr;
}
}  // namespace google::protobuf::field_extraction::testing
