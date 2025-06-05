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
#ifndef GRPC_TRANSCODING_REQUEST_STREAM_TRANSLATOR_H_
#define GRPC_TRANSCODING_REQUEST_STREAM_TRANSLATOR_H_

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>

#include "absl/strings/string_view.h"
#include "google/protobuf/util/converter/object_writer.h"
#include "google/protobuf/util/type_resolver.h"
#include "message_stream.h"
#include "request_message_translator.h"

namespace google {
namespace grpc {

namespace transcoding {

// Translates ObjectWriter events into protobuf messages for streaming requests.
// RequestStreamTranslator handles the outermost array and for each element uses
// a RequestMessageTranslator to translate it to a proto message. Collects the
// translated messages into a deque and exposes those through MessageStream
// interface.
// Example:
//   RequestMessageTranslator t(type_resolver, true, std::move(request_info));
//
//   t.StartList("");
//   ...
//   t.StartObject("");
//   write object 1
//   t.EndObject();
//   ...
//   t.StartObject("");
//   write object 2
//   t.EndObject();
//   ...
//   t.EndList();
//
//   if (!t.Status().ok()) {
//     printf("Error: %s\n", t->Status().ErrorMessage().as_string().c_str());
//     return;
//   }
//
//   std::string message;
//   while (t.NextMessage(&message)) {
//     printf("Message=%s\n", message.c_str());
//   }
//
class RequestStreamTranslator
    : public google::protobuf::util::converter::ObjectWriter,
      public MessageStream {
 public:
  RequestStreamTranslator(google::protobuf::util::TypeResolver& type_resolver,
                          bool output_delimiters, RequestInfo request_info);
  ~RequestStreamTranslator();

  // MessageStream methods
  bool NextMessage(std::string* message);
  bool Finished() const;
  absl::Status Status() const { return status_; }

 private:
  // ObjectWriter methods.
  RequestStreamTranslator* StartObject(absl::string_view name);
  RequestStreamTranslator* EndObject();
  RequestStreamTranslator* StartList(absl::string_view name);
  RequestStreamTranslator* EndList();
  RequestStreamTranslator* RenderBool(absl::string_view name, bool value);
  RequestStreamTranslator* RenderInt32(absl::string_view name, int32_t value);
  RequestStreamTranslator* RenderUint32(absl::string_view name, uint32_t value);
  RequestStreamTranslator* RenderInt64(absl::string_view name, int64_t value);
  RequestStreamTranslator* RenderUint64(absl::string_view name, uint64_t value);
  RequestStreamTranslator* RenderDouble(absl::string_view name, double value);
  RequestStreamTranslator* RenderFloat(absl::string_view name, float value);
  RequestStreamTranslator* RenderString(absl::string_view name,
                                        absl::string_view value);
  RequestStreamTranslator* RenderBytes(absl::string_view name,
                                       absl::string_view value);
  RequestStreamTranslator* RenderNull(absl::string_view name);

  // Sets up the ProtoMessageHelper to handle writing data.
  void StartMessageTranslator();

  // Closes down the ProtoMessageHelper and stores its message.
  void EndMessageTranslator();

  // Helper method to render a single piece of data, to reuse code.
  void RenderData(absl::string_view name, std::function<void()> renderer);

  // TypeResolver to be passed to the RequestMessageTranslator
  google::protobuf::util::TypeResolver& type_resolver_;

  // The status of the translation
  absl::Status status_;

  // The request info
  RequestInfo request_info_;

  // Whether to prefix each message with a delimiter or not
  bool output_delimiters_;

  // The ProtoMessageWriter that is currently writing a message, or null if we
  // are at the root or have invalid input.
  std::unique_ptr<RequestMessageTranslator> translator_;

  // Holds the messages we've translated so far.
  std::deque<std::string> messages_;

  // Depth within the object tree. We special case the root level.
  int depth_;

  // Done with the translation (i.e., have seen the last EndList())
  bool done_;

  RequestStreamTranslator(const RequestStreamTranslator&) = delete;
  RequestStreamTranslator& operator=(const RequestStreamTranslator&) = delete;
};

}  // namespace transcoding

}  // namespace grpc
}  // namespace google
#endif  // GRPC_TRANSCODING_REQUEST_STREAM_TRANSLATOR_H_
