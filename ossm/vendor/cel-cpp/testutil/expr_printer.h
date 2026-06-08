// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_TESTUTIL_EXPR_PRINTER_H_
#define THIRD_PARTY_CEL_CPP_TESTUTIL_EXPR_PRINTER_H_

#include <string>

#include "cel/expr/syntax.pb.h"
#include "common/expr.h"

namespace cel::test {

// Interface for adding additional information to an expression during
// printing.
class ExpressionAdorner {
 public:
  virtual ~ExpressionAdorner() = default;
  virtual std::string Adorn(const Expr& e) const = 0;
  virtual std::string AdornStructField(const StructExprField& e) const = 0;
  virtual std::string AdornMapEntry(const MapExprEntry& e) const = 0;
};

// Default implementation of the ExpressionAdorner which does nothing.
const ExpressionAdorner& EmptyAdorner();

// Helper class for printing an expression AST to a human readable, but detailed
// and consistently formatted string.
//
// Note: this implementation is recursive and is not suitable for printing
// arbitrarily large expressions.
class ExprPrinter {
 public:
  ExprPrinter() : adorner_(EmptyAdorner()) {}
  explicit ExprPrinter(const ExpressionAdorner& adorner) : adorner_(adorner) {}

  std::string PrintProto(const cel::expr::Expr& expr) const;
  std::string Print(const Expr& expr) const;

 private:
  const ExpressionAdorner& adorner_;
};

}  // namespace cel::test

#endif  // THIRD_PARTY_CEL_CPP_TESTUTIL_EXPR_PRINTER_H_
