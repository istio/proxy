// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "grpc_transcoding/message_reader.h"

#include <memory>

#include "google/protobuf/io/zero_copy_stream_impl.h"

namespace google {
namespace grpc {

namespace transcoding {

namespace pb = ::google::protobuf;
namespace pbio = ::google::protobuf::io;

MessageReader::MessageReader(TranscoderInputStream* in)
    : in_(in),
      current_message_size_(0),
      have_current_message_size_(false),
      finished_(false) {}

namespace {

// A helper function that reads the given number of bytes from a
// ZeroCopyInputStream and copies it to the given buffer
bool ReadStream(pbio::ZeroCopyInputStream* stream, unsigned char* buffer,
                int size) {
  int size_in = 0;
  const void* data_in = nullptr;
  // While we have bytes to read
  while (size > 0) {
    if (!stream->Next(&data_in, &size_in)) {
      return false;
    }
    int to_copy = std::min(size, size_in);
    memcpy(buffer, data_in, to_copy);
    // Advance buffer and subtract the size to reflect the number of bytes left
    buffer += to_copy;
    size -= to_copy;
    // Keep track of uncopied bytes
    size_in -= to_copy;
  }
  // Return the uncopied bytes
  stream->BackUp(size_in);
  return true;
}

// A helper function to extract the size from a gRPC wire format message
// delimiter - see http://www.grpc.io/docs/guides/wire.html.
uint32_t DelimiterToSize(const unsigned char* delimiter) {
  unsigned size = 0;
  // Bytes 1-4 are big-endian 32-bit message size
  size = size | static_cast<unsigned>(delimiter[1]);
  size <<= 8;
  size = size | static_cast<unsigned>(delimiter[2]);
  size <<= 8;
  size = size | static_cast<unsigned>(delimiter[3]);
  size <<= 8;
  size = size | static_cast<unsigned>(delimiter[4]);
  return size;
}

}  // namespace

std::unique_ptr<pbio::ZeroCopyInputStream> MessageReader::NextMessage() {
  if (Finished()) {
    // The stream has ended
    return nullptr;
  }

  // Check if we have the current message size. If not try to read it.
  if (!have_current_message_size_) {
    if (in_->BytesAvailable() <
        static_cast<pb::int64>(kGrpcDelimiterByteSize)) {
      // We don't have 5 bytes available to read the length of the message.
      // Find out whether the stream is finished and return false.
      finished_ = in_->Finished();
      if (finished_ && in_->BytesAvailable() != 0) {
        status_ = absl::Status(absl::StatusCode::kInternal,
                               "Incomplete gRPC frame header received");
      }
      return nullptr;
    }

    // Try to read the delimiter.
    memset(delimiter_, 0, kGrpcDelimiterByteSize);
    if (!ReadStream(in_, delimiter_, kGrpcDelimiterByteSize)) {
      finished_ = true;
      return nullptr;
    }

    if (delimiter_[0] != 0) {
      status_ = absl::Status(
          absl::StatusCode::kInternal,
          "Unsupported gRPC frame flag: " + std::to_string(delimiter_[0]));
      return nullptr;
    }

    current_message_size_ = DelimiterToSize(delimiter_);
    have_current_message_size_ = true;
  }

  if (in_->BytesAvailable() < static_cast<pb::int64>(current_message_size_)) {
    if (in_->Finished()) {
      status_ = absl::Status(
          absl::StatusCode::kInternal,
          "Incomplete gRPC frame expected size: " +
              std::to_string(current_message_size_) +
              " actual size: " + std::to_string(in_->BytesAvailable()));
    }
    // We don't have a full message
    return nullptr;
  }

  // Reset the have_current_message_size_ for the next message
  have_current_message_size_ = false;

  // We have a message! Use LimitingInputStream to wrap the input stream and
  // limit it to current_message_size_ bytes to cover only the current message.
  return std::unique_ptr<pbio::ZeroCopyInputStream>(
      new pbio::LimitingInputStream(in_, current_message_size_));
}

MessageAndGrpcFrame MessageReader::NextMessageAndGrpcFrame() {
  MessageAndGrpcFrame out;
  out.message = NextMessage();
  memcpy(out.grpc_frame, delimiter_, kGrpcDelimiterByteSize);
  out.message_size = current_message_size_;
  return out;
}

}  // namespace transcoding

}  // namespace grpc
}  // namespace google
