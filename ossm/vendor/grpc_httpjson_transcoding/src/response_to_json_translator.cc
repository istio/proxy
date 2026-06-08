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
#include "grpc_transcoding/response_to_json_translator.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/util/type_resolver.h"

namespace google {
namespace grpc {

namespace transcoding {
ResponseToJsonTranslator::ResponseToJsonTranslator(
    ::google::protobuf::util::TypeResolver* type_resolver, std::string type_url,
    bool streaming, TranscoderInputStream* in,
    const JsonResponseTranslateOptions& options)
    : type_resolver_(type_resolver),
      type_url_(std::move(type_url)),
      options_(options),
      streaming_(streaming),
      reader_(in),
      first_(true),
      finished_(false) {}

bool ResponseToJsonTranslator::NextMessage(std::string* message) {
  if (Finished()) {
    // All done
    return false;
  }

  // Try to read a message
  auto proto_in = reader_.NextMessage();
  status_ = reader_.Status();
  if (!status_.ok()) {
    return false;
  }

  if (proto_in) {
    std::string json_out;
    if (TranslateMessage(proto_in.get(), &json_out)) {
      *message = std::move(json_out);
      if (!streaming_) {
        // This is a non-streaming call, so we don't expect more messages.
        finished_ = true;
      }
      return true;
    } else {
      // TranslateMessage() failed - return false. The error details are stored
      // in status_.
      return false;
    }
  } else if (streaming_ && reader_.Finished()) {
    if (!options_.stream_newline_delimited &&
        !options_.stream_sse_style_delimited) {
      // This is a non-newline-delimited and non-SSE-style-delimited streaming
      // call and the input is finished. Return the final ']' or "[]" in case
      // this was an empty stream.
      *message = first_ ? "[]" : "]";
    }
    finished_ = true;
    return true;
  } else {
    // Don't have an input message
    return false;
  }
}

namespace {

// A helper to write a single char to a ZeroCopyOutputStream
bool WriteChar(::google::protobuf::io::ZeroCopyOutputStream* stream, char c) {
  int size = 0;
  void* data = 0;
  if (!stream->Next(&data, &size) || 0 == size) {
    return false;
  }
  // Write the char to the first byte of the buffer and return the rest size-1
  // bytes to the stream.
  *reinterpret_cast<char*>(data) = c;
  stream->BackUp(size - 1);
  return true;
}

// A helper to write a string to a ZeroCopyOutputStream.
bool WriteString(::google::protobuf::io::ZeroCopyOutputStream* stream,
                 const std::string& str) {
  int bytes_to_write = str.size();
  int bytes_written = 0;
  while (bytes_written < bytes_to_write) {
    int size = 0;
    void* data;
    if (!stream->Next(&data, &size) || size == 0) {
      return false;
    }
    int bytes_to_write_this_iteration =
        std::min(bytes_to_write - bytes_written, size);
    memcpy(data, str.data() + bytes_written, bytes_to_write_this_iteration);
    bytes_written += bytes_to_write_this_iteration;
    if (bytes_to_write_this_iteration < size) {
      stream->BackUp(size - bytes_to_write_this_iteration);
    }
  }
  return true;
}

}  // namespace

bool ResponseToJsonTranslator::TranslateMessage(
    ::google::protobuf::io::ZeroCopyInputStream* proto_in,
    std::string* json_out) {
  ::google::protobuf::io::StringOutputStream json_stream(json_out);

  if (streaming_ && options_.stream_sse_style_delimited) {
    if (!WriteString(&json_stream, "data: ")) {
      status_ = absl::Status(absl::StatusCode::kInternal,
                             "Failed to build the response message.");
      return false;
    }
  } else if (streaming_ && !options_.stream_newline_delimited) {
    if (first_) {
      // This is a non-newline-delimited streaming call and this is the first
      // message, so prepend the
      // output JSON with a '['.
      if (!WriteChar(&json_stream, '[')) {
        status_ = absl::Status(absl::StatusCode::kInternal,
                               "Failed to build the response message.");
        return false;
      }
      first_ = false;
    } else {
      // For non-newline-delimited streaming calls add a ',' before each message
      // except the first.
      if (!WriteChar(&json_stream, ',')) {
        status_ = absl::Status(absl::StatusCode::kInternal,
                               "Failed to build the response message.");
        return false;
      }
    }
  }

  // Do the actual translation.
  status_ = ::google::protobuf::util::BinaryToJsonStream(
      type_resolver_, type_url_, proto_in, &json_stream,
      options_.json_print_options);

  if (!status_.ok()) {
    return false;
  }

  // Append a newline delimiter after the message if needed.
  if (streaming_ && options_.stream_sse_style_delimited) {
    if (!WriteString(&json_stream, "\n\n")) {
      status_ = absl::Status(absl::StatusCode::kInternal,
                             "Failed to build the response message.");
      return false;
    }
  } else if (streaming_ && options_.stream_newline_delimited) {
    if (!WriteChar(&json_stream, '\n')) {
      status_ = absl::Status(absl::StatusCode::kInternal,
                             "Failed to build the response message.");
      return false;
    }
  }

  return true;
}

}  // namespace transcoding

}  // namespace grpc
}  // namespace google
