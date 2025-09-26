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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_TESTING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_TESTING_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "internal/equals_text_proto.h"
#include "internal/parse_text_proto.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

// GTest Printer
void PrintTo(const Value& value, std::ostream* os);

namespace test {

using ValueMatcher = testing::Matcher<Value>;

MATCHER_P(ValueKindIs, m, "") {
  return ExplainMatchResult(m, arg.kind(), result_listener);
}

// Returns a matcher for CEL null value.
inline ValueMatcher IsNullValue() { return ValueKindIs(ValueKind::kNull); }

// Returns a matcher for CEL bool values.
ValueMatcher BoolValueIs(testing::Matcher<bool> m);

// Returns a matcher for CEL int values.
ValueMatcher IntValueIs(testing::Matcher<int64_t> m);

// Returns a matcher for CEL uint values.
ValueMatcher UintValueIs(testing::Matcher<uint64_t> m);

// Returns a matcher for CEL double values.
ValueMatcher DoubleValueIs(testing::Matcher<double> m);

// Returns a matcher for CEL duration values.
ValueMatcher DurationValueIs(testing::Matcher<absl::Duration> m);

// Returns a matcher for CEL timestamp values.
ValueMatcher TimestampValueIs(testing::Matcher<absl::Time> m);

// Returns a matcher for CEL error values.
ValueMatcher ErrorValueIs(testing::Matcher<absl::Status> m);

// Returns a matcher for CEL string values.
ValueMatcher StringValueIs(testing::Matcher<std::string> m);

// Returns a matcher for CEL bytes values.
ValueMatcher BytesValueIs(testing::Matcher<std::string> m);

// Returns a matcher for CEL map values.
ValueMatcher MapValueIs(testing::Matcher<MapValue> m);

// Returns a matcher for CEL list values.
ValueMatcher ListValueIs(testing::Matcher<ListValue> m);

// Returns a matcher for CEL struct values.
ValueMatcher StructValueIs(testing::Matcher<StructValue> m);

// Returns a matcher for CEL struct values.
ValueMatcher OptionalValueIsEmpty();

// Returns a matcher for CEL struct values.
ValueMatcher OptionalValueIs(ValueMatcher m);

// Returns a Matcher that tests the value of a CEL struct's field.
// ValueManager* mgr must remain valid for the lifetime of the matcher.
MATCHER_P5(StructValueFieldIs, name, m, descriptor_pool, message_factory, arena,
           "") {
  auto wrapped_m = ::absl_testing::IsOkAndHolds(m);

  return ExplainMatchResult(wrapped_m,
                            cel::StructValue(arg).GetFieldByName(
                                name, descriptor_pool, message_factory, arena),
                            result_listener);
}

// Returns a Matcher that tests the presence of a CEL struct's field.
// ValueManager* mgr must remain valid for the lifetime of the matcher.
MATCHER_P2(StructValueFieldHas, name, m, "") {
  auto wrapped_m = ::absl_testing::IsOkAndHolds(m);

  return ExplainMatchResult(
      wrapped_m, cel::StructValue(arg).HasFieldByName(name), result_listener);
}

class ListValueElementsMatcher {
 public:
  using is_gtest_matcher = void;

  explicit ListValueElementsMatcher(
      testing::Matcher<std::vector<Value>>&& m,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::MessageFactory* absl_nonnull message_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : m_(std::move(m)),
        descriptor_pool_(ABSL_DIE_IF_NULL(descriptor_pool)),  // Crash OK
        message_factory_(ABSL_DIE_IF_NULL(message_factory)),  // Crash OK
        arena_(ABSL_DIE_IF_NULL(arena))                       // Crash OK
  {}

  bool MatchAndExplain(const ListValue& arg,
                       testing::MatchResultListener* result_listener) const {
    std::vector<Value> elements;
    absl::Status s = arg.ForEach(
        [&](const Value& v) -> absl::StatusOr<bool> {
          elements.push_back(v);
          return true;
        },
        descriptor_pool_, message_factory_, arena_);
    if (!s.ok()) {
      *result_listener << "cannot convert to list of values: " << s;
      return false;
    }
    return m_.MatchAndExplain(elements, result_listener);
  }

  void DescribeTo(std::ostream* os) const { *os << m_; }
  void DescribeNegationTo(std::ostream* os) const { *os << m_; }

 private:
  testing::Matcher<std::vector<Value>> m_;
  const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool_;
  google::protobuf::MessageFactory* absl_nonnull message_factory_;
  google::protobuf::Arena* absl_nonnull arena_;
};

// Returns a matcher that tests the elements of a cel::ListValue on a given
// matcher as if they were a std::vector<cel::Value>.
// ValueManager* mgr must remain valid for the lifetime of the matcher.
inline ListValueElementsMatcher ListValueElements(
    testing::Matcher<std::vector<Value>>&& m,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return ListValueElementsMatcher(std::move(m), descriptor_pool,
                                  message_factory, arena);
}

class MapValueElementsMatcher {
 public:
  using is_gtest_matcher = void;

  explicit MapValueElementsMatcher(
      testing::Matcher<std::vector<std::pair<Value, Value>>>&& m,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::MessageFactory* absl_nonnull message_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : m_(std::move(m)),
        descriptor_pool_(ABSL_DIE_IF_NULL(descriptor_pool)),  // Crash OK
        message_factory_(ABSL_DIE_IF_NULL(message_factory)),  // Crash OK
        arena_(ABSL_DIE_IF_NULL(arena))                       // Crash OK
  {}

  bool MatchAndExplain(const MapValue& arg,
                       testing::MatchResultListener* result_listener) const {
    std::vector<std::pair<Value, Value>> elements;
    absl::Status s = arg.ForEach(
        [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
          elements.push_back({key, value});
          return true;
        },
        descriptor_pool_, message_factory_, arena_);
    if (!s.ok()) {
      *result_listener << "cannot convert to list of values: " << s;
      return false;
    }
    return m_.MatchAndExplain(elements, result_listener);
  }

  void DescribeTo(std::ostream* os) const { *os << m_; }
  void DescribeNegationTo(std::ostream* os) const { *os << m_; }

 private:
  testing::Matcher<std::vector<std::pair<Value, Value>>> m_;
  const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool_;
  google::protobuf::MessageFactory* absl_nonnull message_factory_;
  google::protobuf::Arena* absl_nonnull arena_;
};

// Returns a matcher that tests the elements of a cel::MapValue on a given
// matcher as if they were a std::vector<std::pair<cel::Value, cel::Value>>.
// ValueManager* mgr must remain valid for the lifetime of the matcher.
inline MapValueElementsMatcher MapValueElements(
    testing::Matcher<std::vector<std::pair<Value, Value>>>&& m,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return MapValueElementsMatcher(std::move(m), descriptor_pool, message_factory,
                                 arena);
}

}  // namespace test

}  // namespace cel

namespace cel::common_internal {

template <typename... Ts>
class ValueTest : public ::testing::TestWithParam<std::tuple<Ts...>> {
 public:
  google::protobuf::Arena* absl_nonnull arena() { return &arena_; }

  const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool() {
    return ::cel::internal::GetTestingDescriptorPool();
  }

  google::protobuf::MessageFactory* absl_nonnull message_factory() {
    return ::cel::internal::GetTestingMessageFactory();
  }

  google::protobuf::Message* absl_nonnull NewArenaValueMessage() {
    return ABSL_DIE_IF_NULL(                                      // Crash OK
               message_factory()->GetPrototype(ABSL_DIE_IF_NULL(  // Crash OK
                   descriptor_pool()->FindMessageTypeByName(
                       "google.protobuf.Value"))))
        ->New(arena());
  }

  template <typename T>
  auto GeneratedParseTextProto(absl::string_view text = "") {
    return ::cel::internal::GeneratedParseTextProto<T>(
        arena(), text, descriptor_pool(), message_factory());
  }

  template <typename T>
  auto DynamicParseTextProto(absl::string_view text = "") {
    return ::cel::internal::DynamicParseTextProto<T>(
        arena(), text, descriptor_pool(), message_factory());
  }

  template <typename T>
  auto EqualsTextProto(absl::string_view text) {
    return ::cel::internal::EqualsTextProto<T>(arena(), text, descriptor_pool(),
                                               message_factory());
  }

  auto EqualsValueTextProto(absl::string_view text) {
    return EqualsTextProto<google::protobuf::Value>(text);
  }

  template <typename T>
  const google::protobuf::FieldDescriptor* absl_nonnull DynamicGetField(
      absl::string_view name) {
    return ABSL_DIE_IF_NULL(                                        // Crash OK
        ABSL_DIE_IF_NULL(descriptor_pool()->FindMessageTypeByName(  // Crash OK
                             internal::MessageTypeNameFor<T>()))
            ->FindFieldByName(name));
  }

  template <typename T>
  ParsedMessageValue MakeParsedMessage(absl::string_view text = R"pb()pb") {
    return ParsedMessageValue(DynamicParseTextProto<T>(text), arena());
  }

 private:
  google::protobuf::Arena arena_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_TESTING_H_
