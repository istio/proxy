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
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/memory_testing.h"
#include "common/type_factory.h"
#include "common/type_introspector.h"
#include "common/type_manager.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "internal/testing.h"

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
MATCHER_P3(StructValueFieldIs, mgr, name, m, "") {
  auto wrapped_m = ::absl_testing::IsOkAndHolds(m);

  return ExplainMatchResult(wrapped_m,
                            cel::StructValue(arg).GetFieldByName(*mgr, name),
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

  explicit ListValueElementsMatcher(cel::ValueManager* mgr,
                                    testing::Matcher<std::vector<Value>>&& m)
      : mgr_(*mgr), m_(std::move(m)) {}

  bool MatchAndExplain(const ListValue& arg,
                       testing::MatchResultListener* result_listener) const {
    std::vector<Value> elements;
    absl::Status s =
        arg.ForEach(mgr_, [&](const Value& v) -> absl::StatusOr<bool> {
          elements.push_back(v);
          return true;
        });
    if (!s.ok()) {
      *result_listener << "cannot convert to list of values: " << s;
      return false;
    }
    return m_.MatchAndExplain(elements, result_listener);
  }

  void DescribeTo(std::ostream* os) const { *os << m_; }
  void DescribeNegationTo(std::ostream* os) const { *os << m_; }

 private:
  ValueManager& mgr_;
  testing::Matcher<std::vector<Value>> m_;
};

// Returns a matcher that tests the elements of a cel::ListValue on a given
// matcher as if they were a std::vector<cel::Value>.
// ValueManager* mgr must remain valid for the lifetime of the matcher.
inline ListValueElementsMatcher ListValueElements(
    ValueManager* mgr, testing::Matcher<std::vector<Value>>&& m) {
  return ListValueElementsMatcher(mgr, std::move(m));
}

class MapValueElementsMatcher {
 public:
  using is_gtest_matcher = void;

  explicit MapValueElementsMatcher(
      cel::ValueManager* mgr,
      testing::Matcher<std::vector<std::pair<Value, Value>>>&& m)
      : mgr_(*mgr), m_(std::move(m)) {}

  bool MatchAndExplain(const MapValue& arg,
                       testing::MatchResultListener* result_listener) const {
    std::vector<std::pair<Value, Value>> elements;
    absl::Status s = arg.ForEach(
        mgr_,
        [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
          elements.push_back({key, value});
          return true;
        });
    if (!s.ok()) {
      *result_listener << "cannot convert to list of values: " << s;
      return false;
    }
    return m_.MatchAndExplain(elements, result_listener);
  }

  void DescribeTo(std::ostream* os) const { *os << m_; }
  void DescribeNegationTo(std::ostream* os) const { *os << m_; }

 private:
  ValueManager& mgr_;
  testing::Matcher<std::vector<std::pair<Value, Value>>> m_;
};

// Returns a matcher that tests the elements of a cel::MapValue on a given
// matcher as if they were a std::vector<std::pair<cel::Value, cel::Value>>.
// ValueManager* mgr must remain valid for the lifetime of the matcher.
inline MapValueElementsMatcher MapValueElements(
    ValueManager* mgr,
    testing::Matcher<std::vector<std::pair<Value, Value>>>&& m) {
  return MapValueElementsMatcher(mgr, std::move(m));
}

}  // namespace test

}  // namespace cel

namespace cel::common_internal {

template <typename... Ts>
class ThreadCompatibleValueTest : public ThreadCompatibleMemoryTest<Ts...> {
 private:
  using Base = ThreadCompatibleMemoryTest<Ts...>;

 public:
  void SetUp() override {
    Base::SetUp();
    value_manager_ = NewThreadCompatibleValueManager(
        this->memory_manager(), NewTypeReflector(this->memory_manager()));
  }

  void TearDown() override {
    value_manager_.reset();
    Base::TearDown();
  }

  ValueManager& value_manager() const { return **value_manager_; }

  TypeFactory& type_factory() const { return value_manager(); }

  TypeManager& type_manager() const { return value_manager(); }

  ValueFactory& value_factory() const { return value_manager(); }

 private:
  virtual Shared<TypeReflector> NewTypeReflector(
      MemoryManagerRef memory_manager) {
    return NewThreadCompatibleTypeReflector(memory_manager);
  }

  absl::optional<Shared<ValueManager>> value_manager_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_TESTING_H_
