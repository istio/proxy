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

#ifndef PROTO_FIELD_EXTRACTION_SRC_MESSAGE_DATA_CORD_MESSAGE_DATA_H_
#define PROTO_FIELD_EXTRACTION_SRC_MESSAGE_DATA_CORD_MESSAGE_DATA_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/message_data/message_data.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace google::protobuf::field_extraction {

// The absl::Cord-based CodedInputStreamWrapper.
class CordCodedInputStreamWrapper : public CodedInputStreamWrapper {
 public:
  explicit CordCodedInputStreamWrapper(const absl::Cord& source)
      : cord_input_stream_(&source), coded_input_stream_(&cord_input_stream_) {}

  google::protobuf::io::CodedInputStream& Get() override { return coded_input_stream_; }

 private:
  google::protobuf::io::CordInputStream cord_input_stream_;

  google::protobuf::io::CodedInputStream coded_input_stream_;
};

// The absl::Cord-based MessageData.
class CordMessageData : public MessageData {
 public:
  explicit CordMessageData(absl::Cord cord) : cord_(std::move(cord)) {}

  CordMessageData() = default;

  ~CordMessageData() override = default;

  std::unique_ptr<google::protobuf::io::ZeroCopyInputStream> CreateZeroCopyInputStream()
      const override {
    return std::make_unique<google::protobuf::io::CordInputStream>(&cord_);
  }

  std::unique_ptr<google::protobuf::field_extraction::CodedInputStreamWrapper>
  CreateCodedInputStreamWrapper() const override {
    return std::make_unique<CordCodedInputStreamWrapper>(cord_);
  }

  void AppendExternalMemory(const char* ptr, int64_t n) override {
    // Provide empty data releasor so that the caller fully manages the data
    // lifecycle.
    //
    // absl::Cord::AppendExternalMemory isn't available in absl OSS version so
    // replace with a workaround of appending a cord created by
    // MakeCordFromExternal. For details, see yaqs/3660248920098865152.
    cord_.Append(absl::MakeCordFromExternal(absl::string_view(ptr, n),
                                            [](absl::string_view) {}));
  }

  void RemoveSuffix(size_t n) override { cord_.RemoveSuffix(n); }

  int64_t Size() const override { return cord_.size(); }

  absl::Cord& Cord() { return cord_; }

  absl::Cord SubData(size_t pos, size_t new_size) const override {
    return cord_.Subcord(pos, new_size);
  }

  absl::Cord ToCord() const override { return cord_; }

  void CopyFrom(const absl::Cord& other) override {
    cord_.Clear();
    cord_.Append(other);
  }

  void Append(const absl::Cord& other) override { cord_.Append(other); }

  bool IsEmpty() const override { return cord_.empty(); }

 private:
  absl::Cord cord_;
};

}  // namespace google::protobuf::field_extraction

#endif  // PROTO_FIELD_EXTRACTION_SRC_MESSAGE_DATA_CORD_MESSAGE_DATA_H_
