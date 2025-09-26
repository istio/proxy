/* Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef GRPC_TRANSCODING_MESSAGE_READER_H_
#define GRPC_TRANSCODING_MESSAGE_READER_H_

#include <memory>

#include "absl/status/status.h"
#include "transcoder_input_stream.h"

namespace google {
namespace grpc {

namespace transcoding {

// The number of bytes in the delimiter for gRPC wire format's
// `Length-Prefixed-Message`.
constexpr size_t kGrpcDelimiterByteSize = 5;

// Return type that contains both the proto message and the preceding gRPC data
// frame.
struct MessageAndGrpcFrame {
  std::unique_ptr<::google::protobuf::io::ZeroCopyInputStream> message;
  unsigned char grpc_frame[kGrpcDelimiterByteSize];
  // The size (in bytes) of the full gRPC message, excluding the frame header.
  uint32_t message_size;
};

// MessageReader helps extract full messages from a ZeroCopyInputStream of
// messages in gRPC wire format (http://www.grpc.io/docs/guides/wire.html). Each
// message is returned in a ZeroCopyInputStream. MessageReader doesn't advance
// the underlying ZeroCopyInputStream unless there is a full message available.
// This is done to avoid copying while buffering.
//
// Example:
//   MessageReader reader(&input);
//
//   while (!reader.Finished()) {
//     auto message = reader.NextMessage();
//     if (!message) {
//       // No message is available at this moment.
//       break;
//     }
//
//     const void* buffer = nullptr;
//     int size = 0;
//     while (message.Next(&buffer, &size)) {
//       // Process the message data.
//       ...
//     }
//   }
//
// NOTE: MessageReader is unable to recognize the case when there is an
//       incomplete message at the end of the input. The callers will need to
//       detect it and act appropriately.
//       This is because the MessageReader doesn't call Next() on the input
//       stream until there is a full message available. So, if there is an
//       incomplete message at the end of the input, MessageReader won't call
//       Next() and won't know that the stream has finished.
//
class MessageReader {
 public:
  MessageReader(TranscoderInputStream* in);

  // If a full message is available, NextMessage() returns a ZeroCopyInputStream
  // over the message. Otherwise returns nullptr - this might be temporary, the
  // caller can call NextMessage() again later to check.
  // NOTE: the caller must consume the entire message before calling
  //       NextMessage() again.
  //       That's because the returned ZeroCopyInputStream is a wrapper on top
  //       of the original ZeroCopyInputStream and the MessageReader relies on
  //       the caller to advance the stream to the next message before calling
  //       NextMessage() again.
  // NOTE: the caller should check `Status()` is OK after calling this method.
  std::unique_ptr<::google::protobuf::io::ZeroCopyInputStream> NextMessage();

  // An overload that also outputs the gRPC message delimiter for the parsed
  // message. The caller is free to take ownership of contents in `grpc_frame`.
  // NOTE: the caller must check the `message` is NOT nullptr and the `Status()`
  //       is OK before consuming the `grpc_frame`.
  MessageAndGrpcFrame NextMessageAndGrpcFrame();

  absl::Status Status() const { return status_; }

  // Returns true if the stream has ended (this is permanent); otherwise returns
  // false.
  bool Finished() const { return finished_ || !status_.ok(); }

 private:
  TranscoderInputStream* in_;
  // The size of the current message.
  uint32_t current_message_size_;
  // Whether we have read the current message size or not
  bool have_current_message_size_;
  // Are we all done?
  bool finished_;
  // Status
  absl::Status status_;
  // Buffer to store the current delimiter value.
  unsigned char delimiter_[kGrpcDelimiterByteSize];

  MessageReader(const MessageReader&) = delete;
  MessageReader& operator=(const MessageReader&) = delete;
};

}  // namespace transcoding

}  // namespace grpc
}  // namespace google

#endif  // GRPC_TRANSCODING_MESSAGE_READER_H_
