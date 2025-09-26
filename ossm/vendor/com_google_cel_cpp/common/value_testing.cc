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

#include "common/value_testing.h"

#include <cstdint>
#include <ostream>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "internal/testing.h"

namespace cel {

void PrintTo(const Value& value, std::ostream* os) { *os << value << "\n"; }

namespace test {
namespace {

using ::testing::Matcher;

template <typename Type>
constexpr ValueKind ToValueKind() {
  if constexpr (std::is_same_v<Type, BoolValue>) {
    return ValueKind::kBool;
  } else if constexpr (std::is_same_v<Type, IntValue>) {
    return ValueKind::kInt;
  } else if constexpr (std::is_same_v<Type, UintValue>) {
    return ValueKind::kUint;
  } else if constexpr (std::is_same_v<Type, DoubleValue>) {
    return ValueKind::kDouble;
  } else if constexpr (std::is_same_v<Type, StringValue>) {
    return ValueKind::kString;
  } else if constexpr (std::is_same_v<Type, BytesValue>) {
    return ValueKind::kBytes;
  } else if constexpr (std::is_same_v<Type, DurationValue>) {
    return ValueKind::kDuration;
  } else if constexpr (std::is_same_v<Type, TimestampValue>) {
    return ValueKind::kTimestamp;
  } else if constexpr (std::is_same_v<Type, ErrorValue>) {
    return ValueKind::kError;
  } else if constexpr (std::is_same_v<Type, MapValue>) {
    return ValueKind::kMap;
  } else if constexpr (std::is_same_v<Type, ListValue>) {
    return ValueKind::kList;
  } else if constexpr (std::is_same_v<Type, StructValue>) {
    return ValueKind::kStruct;
  } else if constexpr (std::is_same_v<Type, OpaqueValue>) {
    return ValueKind::kOpaque;
  } else {
    // Otherwise, unspecified (uninitialized value)
    return ValueKind::kError;
  }
}

template <typename Type, typename NativeType>
class SimpleTypeMatcherImpl : public testing::MatcherInterface<const Value&> {
 public:
  using MatcherType = Matcher<NativeType>;

  explicit SimpleTypeMatcherImpl(MatcherType&& matcher)
      : matcher_(std::forward<MatcherType>(matcher)) {}

  bool MatchAndExplain(const Value& v,
                       testing::MatchResultListener* listener) const override {
    return v.Is<Type>() &&
           matcher_.MatchAndExplain(v.Get<Type>().NativeValue(), listener);
  }

  void DescribeTo(std::ostream* os) const override {
    *os << absl::StrCat("kind is ", ValueKindToString(ToValueKind<Type>()),
                        " and ");
    matcher_.DescribeTo(os);
  }

 private:
  MatcherType matcher_;
};

template <typename Type>
class StringTypeMatcherImpl : public testing::MatcherInterface<const Value&> {
 public:
  using MatcherType = Matcher<std::string>;

  explicit StringTypeMatcherImpl(MatcherType matcher)
      : matcher_((std::move(matcher))) {}

  bool MatchAndExplain(const Value& v,
                       testing::MatchResultListener* listener) const override {
    return v.Is<Type>() && matcher_.Matches(v.Get<Type>().ToString());
  }

  void DescribeTo(std::ostream* os) const override {
    *os << absl::StrCat("kind is ", ValueKindToString(ToValueKind<Type>()),
                        " and ");
    matcher_.DescribeTo(os);
  }

 private:
  MatcherType matcher_;
};

template <typename Type>
class AbstractTypeMatcherImpl : public testing::MatcherInterface<const Value&> {
 public:
  using MatcherType = Matcher<Type>;

  explicit AbstractTypeMatcherImpl(MatcherType&& matcher)
      : matcher_(std::forward<MatcherType>(matcher)) {}

  bool MatchAndExplain(const Value& v,
                       testing::MatchResultListener* listener) const override {
    return v.Is<Type>() && matcher_.Matches(v.template Get<Type>());
  }

  void DescribeTo(std::ostream* os) const override {
    *os << absl::StrCat("kind is ", ValueKindToString(ToValueKind<Type>()),
                        " and ");
    matcher_.DescribeTo(os);
  }

 private:
  MatcherType matcher_;
};

class OptionalValueMatcherImpl
    : public testing::MatcherInterface<const Value&> {
 public:
  explicit OptionalValueMatcherImpl(ValueMatcher matcher)
      : matcher_(std::move(matcher)) {}

  bool MatchAndExplain(const Value& v,
                       testing::MatchResultListener* listener) const override {
    if (!v.IsOptional()) {
      *listener << "wanted OptionalValue, got " << ValueKindToString(v.kind());
      return false;
    }
    const auto& optional_value = v.GetOptional();
    if (!optional_value.HasValue()) {
      *listener << "OptionalValue is not engaged";
      return false;
    }
    return matcher_.MatchAndExplain(optional_value.Value(), listener);
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "is OptionalValue that is engaged with value whose ";
    matcher_.DescribeTo(os);
  }

 private:
  ValueMatcher matcher_;
};

MATCHER(OptionalValueIsEmptyImpl, "is empty OptionalValue") {
  const Value& v = arg;
  if (!v.IsOptional()) {
    *result_listener << "wanted OptionalValue, got "
                     << ValueKindToString(v.kind());
    return false;
  }
  const auto& optional_value = v.GetOptional();
  *result_listener << (optional_value.HasValue() ? "is not empty" : "is empty");
  return !optional_value.HasValue();
}

}  // namespace

ValueMatcher BoolValueIs(Matcher<bool> m) {
  return ValueMatcher(new SimpleTypeMatcherImpl<BoolValue, bool>(std::move(m)));
}

ValueMatcher IntValueIs(Matcher<int64_t> m) {
  return ValueMatcher(
      new SimpleTypeMatcherImpl<IntValue, int64_t>(std::move(m)));
}

ValueMatcher UintValueIs(Matcher<uint64_t> m) {
  return ValueMatcher(
      new SimpleTypeMatcherImpl<UintValue, uint64_t>(std::move(m)));
}

ValueMatcher DoubleValueIs(Matcher<double> m) {
  return ValueMatcher(
      new SimpleTypeMatcherImpl<DoubleValue, double>(std::move(m)));
}

ValueMatcher TimestampValueIs(Matcher<absl::Time> m) {
  return ValueMatcher(
      new SimpleTypeMatcherImpl<TimestampValue, absl::Time>(std::move(m)));
}

ValueMatcher DurationValueIs(Matcher<absl::Duration> m) {
  return ValueMatcher(
      new SimpleTypeMatcherImpl<DurationValue, absl::Duration>(std::move(m)));
}

ValueMatcher ErrorValueIs(Matcher<absl::Status> m) {
  return ValueMatcher(
      new SimpleTypeMatcherImpl<ErrorValue, absl::Status>(std::move(m)));
}

ValueMatcher StringValueIs(Matcher<std::string> m) {
  return ValueMatcher(new StringTypeMatcherImpl<StringValue>(std::move(m)));
}

ValueMatcher BytesValueIs(Matcher<std::string> m) {
  return ValueMatcher(new StringTypeMatcherImpl<BytesValue>(std::move(m)));
}

ValueMatcher MapValueIs(Matcher<MapValue> m) {
  return ValueMatcher(new AbstractTypeMatcherImpl<MapValue>(std::move(m)));
}

ValueMatcher ListValueIs(Matcher<ListValue> m) {
  return ValueMatcher(new AbstractTypeMatcherImpl<ListValue>(std::move(m)));
}

ValueMatcher StructValueIs(Matcher<StructValue> m) {
  return ValueMatcher(new AbstractTypeMatcherImpl<StructValue>(std::move(m)));
}

ValueMatcher OptionalValueIs(ValueMatcher m) {
  return ValueMatcher(new OptionalValueMatcherImpl(std::move(m)));
}

ValueMatcher OptionalValueIsEmpty() { return OptionalValueIsEmptyImpl(); }

}  // namespace test

}  // namespace cel
