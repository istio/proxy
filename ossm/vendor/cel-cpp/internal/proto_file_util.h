// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_FILE_UTIL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_FILE_UTIL_H_

#include <cerrno>
#include <cstring>
#include <fstream>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"

namespace cel::internal::test {

// Reads a binary protobuf message of MessageType from the given path.
template <class MessageType>
absl::Status ReadBinaryProtoFromFile(absl::string_view file_name,
                                     MessageType& message) {
  std::ifstream file;
  file.open(std::string(file_name), std::fstream::in | std::fstream::binary);
  if (!file.is_open()) {
    return absl::NotFoundError(absl::StrFormat("Failed to open file '%s': %s",
                                               file_name, strerror(errno)));
  }

  if (!message.ParseFromIstream(&file)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse proto of type '%s' from file '%s'",
                        message.GetTypeName(), file_name));
  }

  return absl::OkStatus();
}

// Reads a text protobuf message of MessageType from the given path.
template <class MessageType>
absl::Status ReadTextProtoFromFile(absl::string_view file_name,
                                   MessageType& message) {
  std::ifstream file;
  file.open(std::string(file_name), std::fstream::in | std::fstream::binary);
  if (!file.is_open()) {
    return absl::NotFoundError(absl::StrFormat("Failed to open file '%s': %s",
                                               file_name, strerror(errno)));
  }

  google::protobuf::io::IstreamInputStream stream(&file);
  if (!google::protobuf::TextFormat::Parse(&stream, &message)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse proto of type '%s' from file '%s'",
                        message.GetTypeName(), file_name));
  }
  return absl::OkStatus();
}

}  // namespace cel::internal::test

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_FILE_UTIL_H_
