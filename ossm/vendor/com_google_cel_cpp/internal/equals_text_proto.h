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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_EQUALS_PROTO_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_EQUALS_PROTO_H_

#include <ostream>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "internal/parse_text_proto.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

namespace cel::internal {

class TextProtoMatcher {
 public:
  TextProtoMatcher(Owned<const google::protobuf::Message> message,
                   absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
                   absl::Nonnull<google::protobuf::MessageFactory*> factory)
      : message_(std::move(message)), pool_(pool), factory_(factory) {}

  void DescribeTo(std::ostream* os) const;

  void DescribeNegationTo(std::ostream* os) const;

  bool MatchAndExplain(const google::protobuf::MessageLite& other,
                       ::testing::MatchResultListener* listener) const;

 private:
  Owned<const google::protobuf::Message> message_;
  absl::Nonnull<const google::protobuf::DescriptorPool*> pool_;
  absl::Nonnull<google::protobuf::MessageFactory*> factory_;
};

template <typename T>
::testing::PolymorphicMatcher<TextProtoMatcher> EqualsTextProto(
    Allocator<> alloc, absl::string_view text,
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool =
        GetTestingDescriptorPool(),
    absl::Nonnull<google::protobuf::MessageFactory*> factory =
        GetTestingMessageFactory()) {
  return ::testing::MakePolymorphicMatcher(TextProtoMatcher(
      DynamicParseTextProto<T>(alloc, text, pool, factory), pool, factory));
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_EQUALS_PROTO_H_
