// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_TOOLS_BRANCH_COVERAGE_H_
#define THIRD_PARTY_CEL_CPP_TOOLS_BRANCH_COVERAGE_H_

#include <cstdint>
#include <memory>

#include "cel/expr/checked.pb.h"
#include "absl/base/attributes.h"
#include "common/value.h"
#include "eval/public/cel_value.h"
#include "tools/navigable_ast.h"

namespace cel {

// Interface for BranchCoverage collection utility.
//
// This provides a factory for instrumentation that collects coverage
// information over multiple executions of a CEL expression. This does not
// provide any mechanism for de-duplicating multiple CheckedExpr instances
// that represent the same expression within or across processes.
//
// The default implementation is thread safe.
//
// TODO(uncreated-issue/65): add support for interesting aggregate stats.
class BranchCoverage {
 public:
  struct NodeCoverageStats {
    bool is_boolean;
    int evaluation_count;
    int boolean_true_count;
    int boolean_false_count;
    int error_count;
  };

  virtual ~BranchCoverage() = default;

  virtual void Record(int64_t expr_id, const Value& value) = 0;
  virtual void RecordLegacyValue(
      int64_t expr_id, const google::api::expr::runtime::CelValue& value) = 0;

  virtual NodeCoverageStats StatsForNode(int64_t expr_id) const = 0;

  virtual const NavigableProtoAst& ast() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;
  virtual const cel::expr::CheckedExpr& expr() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;
};

std::unique_ptr<BranchCoverage> CreateBranchCoverage(
    const cel::expr::CheckedExpr& expr);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_BRANCH_COVERAGE_H_
