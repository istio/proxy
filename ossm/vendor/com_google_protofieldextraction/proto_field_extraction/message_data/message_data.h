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

#ifndef PROTO_FIELD_EXTRACTION_SRC_MESSAGE_DATA_MESSAGE_DATA_H_
#define PROTO_FIELD_EXTRACTION_SRC_MESSAGE_DATA_MESSAGE_DATA_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "absl/strings/cord.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream.h"

namespace google::protobuf::field_extraction {

// CodedInputStreamWrapper is an interface that could generate
// google::protobuf::io::CodedInputStream(not a virtual class)
//
// It helps
// 1) wrap the ownership of the other dependencies that
// CodedInputStream needs to construct with.
//
// 2) abstract the processes of generating a CodedInputStream for different
// MessageData types.
class CodedInputStreamWrapper {
 public:
  virtual google::protobuf::io::CodedInputStream& Get() = 0;

  virtual ~CodedInputStreamWrapper() = default;
};

class CodedInputStreamWrapperFactory {
 public:
  virtual std::unique_ptr<CodedInputStreamWrapper>
  CreateCodedInputStreamWrapper() const = 0;

  virtual ~CodedInputStreamWrapperFactory() = default;
};

// MessageData is an interface representing the underlying data buffer of one
// message. It supports zero-copy read and write on the data buffer.
class MessageData : public CodedInputStreamWrapperFactory {
 public:
  // Append a sequence of characters to the end of the buffer. The caller owns
  // the data so it is the caller's responsibility to
  //
  // 1) ensure that this
  // sequence remains live until all of the data has been consumed from this
  // buffer.
  // 2) release the data.
  virtual void AppendExternalMemory(const char* ptr, int64_t n) = 0;

  virtual std::unique_ptr<google::protobuf::io::ZeroCopyInputStream>
  CreateZeroCopyInputStream() const = 0;

  // Removes the last `n` bytes of the message data.
  virtual void RemoveSuffix(size_t n) = 0;

  // Returns a new Cord with data copy, representing the subrange [pos, pos +
  // new_size).
  // If pos >= size(), the result is empty().
  // If (pos + new_size) >= size(), the result is the subrange [pos, size()).
  virtual absl::Cord SubData(size_t pos, size_t new_size) const = 0;

  // Returns a new Cord with data copy, representing the entire message.
  virtual absl::Cord ToCord() const = 0;

  // Copies data from other Cord to (this*).
  virtual void CopyFrom(const absl::Cord& other) = 0;

  // Appends data to the Cord, which comes from another Cord.
  virtual void Append(const absl::Cord& other) = 0;

  virtual bool IsEmpty() const = 0;

  // The size of MessageData
  virtual int64_t Size() const = 0;

  virtual ~MessageData() = default;
};

}  // namespace google::protobuf::field_extraction

#endif  // PROTO_FIELD_EXTRACTION_SRC_MESSAGE_DATA_MESSAGE_DATA_H_
