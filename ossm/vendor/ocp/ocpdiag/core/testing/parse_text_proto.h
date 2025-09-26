// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_TESTING_PARSE_TEXT_PROTO_H_
#define OCPDIAG_CORE_TESTING_PARSE_TEXT_PROTO_H_

#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"

namespace ocpdiag::testing {

// Replacement for Google ParseTextProtoOrDie.
// Only to be used in unit tests.
// Usage: MyMessage msg = ParseTextProtoOrDie(my_text_proto);
class ParseTextProtoOrDie {
 public:
  ParseTextProtoOrDie(absl::string_view text_proto) : text_proto_(text_proto) {}

  template <class T>
  operator T() {
    T message;
    if (!google::protobuf::TextFormat::ParseFromString(text_proto_, &message)) {
      ADD_FAILURE() << "Failed to parse textproto: " << text_proto_;
      abort();
    }
    return message;
  }

 private:
  std::string text_proto_;
};

}  // namespace ocpdiag::testing

#endif  // OCPDIAG_CORE_TESTING_PARSE_TEXT_PROTO_H_
