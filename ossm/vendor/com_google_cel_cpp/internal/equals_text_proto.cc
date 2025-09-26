// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/equals_text_proto.h"

#include <ostream>
#include <string>

#include "absl/log/absl_check.h"
#include "absl/memory/memory.h"
#include "absl/strings/cord.h"
#include "internal/testing.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"

namespace cel::internal {

void TextProtoMatcher::DescribeTo(std::ostream* os) const {
  std::string text;
  ABSL_CHECK(  // Crash OK
      google::protobuf::TextFormat::PrintToString(*message_, &text));
  *os << "is equal to <" << text << ">";
}

void TextProtoMatcher::DescribeNegationTo(std::ostream* os) const {
  std::string text;
  ABSL_CHECK(  // Crash OK
      google::protobuf::TextFormat::PrintToString(*message_, &text));
  *os << "is not equal to <" << text << ">";
}

bool TextProtoMatcher::MatchAndExplain(
    const google::protobuf::MessageLite& other,
    ::testing::MatchResultListener* listener) const {
  if (other.GetTypeName() != message_->GetTypeName()) {
    if (listener->IsInterested()) {
      *listener << "whose type should be " << message_->GetTypeName()
                << " but actually is " << other.GetTypeName();
    }
    return false;
  }
  google::protobuf::util::MessageDifferencer differencer;
  std::string diff;
  if (listener->IsInterested()) {
    differencer.ReportDifferencesToString(&diff);
  }
  bool match;
  if (const auto* other_full_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Message>(&other);
      other_full_message != nullptr &&
      other_full_message->GetDescriptor() == message_->GetDescriptor()) {
    match = differencer.Compare(*other_full_message, *message_);
  } else {
    auto other_message = absl::WrapUnique(message_->New());
    absl::Cord serialized;
    ABSL_CHECK(other.SerializeToCord(&serialized));        // Crash OK
    ABSL_CHECK(other_message->ParseFromCord(serialized));  // Crash OK
    match = differencer.Compare(*other_message, *message_);
  }
  if (!match && listener->IsInterested()) {
    if (!diff.empty() && diff.back() == '\n') {
      diff.erase(diff.end() - 1);
    }
    *listener << "with the difference:\n" << diff;
  }
  return match;
}

}  // namespace cel::internal
