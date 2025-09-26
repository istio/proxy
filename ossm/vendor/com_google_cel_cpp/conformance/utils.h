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

#ifndef THIRD_PARTY_CEL_CPP_CONFORMANCE_UTILS_H_
#define THIRD_PARTY_CEL_CPP_CONFORMANCE_UTILS_H_

#include <cstdint>
#include <string>

#include "cel/expr/checked.pb.h"
#include "cel/expr/eval.pb.h"
#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "cel/expr/value.pb.h"
#include "absl/log/absl_check.h"
#include "internal/testing.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/field_comparator.h"
#include "google/protobuf/util/message_differencer.h"

namespace cel_conformance {

inline std::string DescribeMessage(const google::protobuf::Message& message) {
  std::string string;
  ABSL_CHECK(google::protobuf::TextFormat::PrintToString(message, &string));
  if (string.empty()) {
    string = "\"\"\n";
  }
  return string;
}

MATCHER_P(MatchesConformanceValue, expected, "") {
  static auto* kFieldComparator = []() {
    auto* field_comparator = new google::protobuf::util::DefaultFieldComparator();
    field_comparator->set_treat_nan_as_equal(true);
    return field_comparator;
  }();
  static auto* kDifferencer = []() {
    auto* differencer = new google::protobuf::util::MessageDifferencer();
    differencer->set_message_field_comparison(
        google::protobuf::util::MessageDifferencer::EQUIVALENT);
    differencer->set_field_comparator(kFieldComparator);
    const auto* descriptor = cel::expr::MapValue::descriptor();
    const auto* entries_field = descriptor->FindFieldByName("entries");
    const auto* key_field =
        entries_field->message_type()->FindFieldByName("key");
    differencer->TreatAsMap(entries_field, key_field);
    return differencer;
  }();

  const cel::expr::ExprValue& got = arg;
  const cel::expr::Value& want = expected;

  cel::expr::ExprValue test_value;
  (*test_value.mutable_value()) = want;

  if (kDifferencer->Compare(got, test_value)) {
    return true;
  }
  (*result_listener) << "got: " << DescribeMessage(got);
  (*result_listener) << "\n";
  (*result_listener) << "wanted: " << DescribeMessage(test_value);
  return false;
}

MATCHER_P(ResultTypeMatches, expected, "") {
  static auto* kDifferencer = []() {
    auto* differencer = new google::protobuf::util::MessageDifferencer();
    differencer->set_message_field_comparison(
        google::protobuf::util::MessageDifferencer::EQUIVALENT);
    return differencer;
  }();

  const cel::expr::Type& want = expected;
  const google::api::expr::v1alpha1::CheckedExpr& checked_expr = arg;

  int64_t root_id = checked_expr.expr().id();
  auto it = checked_expr.type_map().find(root_id);

  if (it == checked_expr.type_map().end()) {
    (*result_listener) << "type map does not contain root id: " << root_id;
    return false;
  }

  auto got_versioned = it->second;
  std::string serialized;
  cel::expr::Type got;
  if (!got_versioned.SerializeToString(&serialized) ||
      !got.ParseFromString(serialized)) {
    (*result_listener) << "type cannot be converted from versioned type: "
                       << DescribeMessage(got_versioned);
    return false;
  }

  if (kDifferencer->Compare(got, want)) {
    return true;
  }
  (*result_listener) << "got: " << DescribeMessage(got);
  (*result_listener) << "\n";
  (*result_listener) << "wanted: " << DescribeMessage(want);
  return false;
}

}  // namespace cel_conformance

#endif  // THIRD_PARTY_CEL_CPP_CONFORMANCE_UTILS_H_
