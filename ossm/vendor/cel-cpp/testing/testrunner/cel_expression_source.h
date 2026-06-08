// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_CEL_EXPRESSION_SOURCE_H_
#define THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_CEL_EXPRESSION_SOURCE_H_

#include <string>
#include <utility>
#include <variant>

#include "cel/expr/checked.pb.h"

namespace cel::test {

// A wrapper class that holds one of three possible sources for a CEL
// expression using a std::variant for type safety.
class CelExpressionSource {
 public:
  // Distinct wrapper types are used for string-based sources to disambiguate
  // them within the std::variant.
  struct RawExpression {
    std::string value;
  };

  struct CelFile {
    std::string path;
  };

  // The variant holds one of the three possible source types.
  using SourceVariant =
      std::variant<cel::expr::CheckedExpr, RawExpression, CelFile>;

  // Creates a CelExpressionSource from a compiled
  // cel::expr::CheckedExpr.
  static CelExpressionSource FromCheckedExpr(
      cel::expr::CheckedExpr checked_expr) {
    return CelExpressionSource(std::move(checked_expr));
  }

  // Creates a CelExpressionSource from a raw CEL expression string.
  static CelExpressionSource FromRawExpression(std::string raw_expression) {
    return CelExpressionSource(RawExpression{std::move(raw_expression)});
  }

  // Creates a CelExpressionSource from a file path pointing to a .cel file.
  static CelExpressionSource FromCelFile(std::string cel_file_path) {
    return CelExpressionSource(CelFile{std::move(cel_file_path)});
  }

  // Make copyable and movable.
  CelExpressionSource(const CelExpressionSource&) = default;
  CelExpressionSource& operator=(const CelExpressionSource&) = default;
  CelExpressionSource(CelExpressionSource&&) = default;
  CelExpressionSource& operator=(CelExpressionSource&&) = default;

  // Returns the underlying variant. The caller is expected to use std::visit
  // to interact with the active value in a type-safe manner.
  const SourceVariant& source() const { return source_; }

 private:
  // A single private constructor enforces creation via the static factories.
  explicit CelExpressionSource(SourceVariant source)
      : source_(std::move(source)) {}

  // A single std::variant member efficiently stores one of the possible states.
  SourceVariant source_;
};
}  // namespace cel::test

#endif  // THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_CEL_EXPRESSION_SOURCE_H_
