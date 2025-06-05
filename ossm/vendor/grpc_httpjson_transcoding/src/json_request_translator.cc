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
#include "grpc_transcoding/json_request_translator.h"

#include <string>

#include "absl/strings/string_view.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/util/converter/json_stream_parser.h"
#include "google/protobuf/util/converter/object_writer.h"
#include "grpc_transcoding/message_stream.h"
#include "grpc_transcoding/request_message_translator.h"
#include "grpc_transcoding/request_stream_translator.h"

namespace google {
namespace grpc {

namespace transcoding {
namespace {

namespace pb = ::google::protobuf;
namespace pbio = ::google::protobuf::io;
namespace pbutil = ::google::protobuf::util;
namespace pbconv = ::google::protobuf::util::converter;

// An on-demand request translation implementation where the reading of the
// input and translation happen only as needed when the caller asks for an
// output message.
//
// LazyRequestTranslator is given
//    - a ZeroCopyInputStream (json_input) to read the input JSON from,
//    - a JsonStreamParser (parser) - the input end of the translation
//      pipeline, i.e. that takes the input JSON,
//    - a MessageStream (translated), the output end of the translation
//      pipeline, i.e. where the output proto messages appear.
// When asked for a message it reads chunks from the input stream and passes
// to the json parser until a message appears in the output (translated)
// stream, or until the input JSON stream runs out of data (in this case, caller
// will call NextMessage again in the future when more data is available).
class LazyRequestTranslator : public MessageStream {
 public:
  LazyRequestTranslator(pbio::ZeroCopyInputStream* json_input,
                        pbconv::JsonStreamParser* json_parser,
                        MessageStream* translated)
      : input_json_(json_input),
        json_parser_(json_parser),
        translated_(translated),
        seen_input_(false) {}

  // MessageStream implementation
  bool NextMessage(std::string* message) {
    // Keep translating chunks until a message appears in the translated stream.
    while (!translated_->NextMessage(message)) {
      if (!TranslateChunk()) {
        // Error or no more input to translate.
        return false;
      }
    }
    return true;
  }
  bool Finished() const { return translated_->Finished() || !status_.ok(); }
  absl::Status Status() const { return status_; }

 private:
  // Translates one chunk of data. Returns true, if there was input to
  // translate; otherwise or in case of an error returns false.
  bool TranslateChunk() {
    if (Finished()) {
      return false;
    }
    // Read the next chunk of data from input_json_
    const void* data = nullptr;
    int size = 0;
    if (!input_json_->Next(&data, &size)) {
      // End of input
      if (!seen_input_) {
        // If there was no input at all translate an empty JSON object ("{}").
        return CheckParsingStatus(json_parser_->Parse("{}"));
      }
      // No more data to translate, finish the parser and return false.
      CheckParsingStatus(json_parser_->FinishParse());
      return false;
    } else if (0 == size) {
      // No data at this point, but there might be more input later.
      return false;
    }
    seen_input_ = true;

    // Feed the chunk to the parser & check the status.
    return CheckParsingStatus(json_parser_->Parse(
        absl::string_view(reinterpret_cast<const char*>(data), size)));
  }

  // If parsing status fails, return false.
  // check translated status, if fails, return false.
  // save failed status.
  bool CheckParsingStatus(absl::Status parsing_status) {
    status_ = parsing_status;
    if (!status_.ok()) {
      return false;
    }
    // Check the translation status
    status_ = translated_->Status();
    if (!status_.ok()) {
      return false;
    }
    return true;
  }

  // The input JSON stream
  pbio::ZeroCopyInputStream* input_json_;

  // The JSON parser that is the starting point of the translation pipeline
  pbconv::JsonStreamParser* json_parser_;

  // The stream where the translated messages appear
  MessageStream* translated_;

  // Whether we have seen any input or not
  bool seen_input_;

  // Translation status
  absl::Status status_;
};

}  // namespace

JsonRequestTranslator::JsonRequestTranslator(
    pbutil::TypeResolver* type_resolver, pbio::ZeroCopyInputStream* json_input,
    RequestInfo request_info, bool streaming, bool output_delimiters) {
  // A writer that accepts input ObjectWriter events for translation
  pbconv::ObjectWriter* writer = nullptr;
  // The stream where translated messages appear
  MessageStream* translated = nullptr;
  if (streaming) {
    // Streaming - we'll need a RequestStreamTranslator
    stream_translator_.reset(new RequestStreamTranslator(
        *type_resolver, output_delimiters, std::move(request_info)));
    writer = stream_translator_.get();
    translated = stream_translator_.get();
  } else {
    // No streaming - use a RequestMessageTranslator
    message_translator_.reset(new RequestMessageTranslator(
        *type_resolver, output_delimiters, std::move(request_info)));
    writer = &message_translator_->Input();
    translated = message_translator_.get();
  }
  parser_.reset(new pbconv::JsonStreamParser(writer));
  output_.reset(
      new LazyRequestTranslator(json_input, parser_.get(), translated));
}

}  // namespace transcoding

}  // namespace grpc
}  // namespace google
