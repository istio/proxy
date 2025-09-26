// Copyright 2023 Google LLC
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
#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_ISSUE_COLLECTOR_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_ISSUE_COLLECTOR_H_

#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "runtime/runtime_issue.h"

namespace cel::runtime_internal {

// IssueCollector collects issues and reports absl::Status according to the
// configured severity limit.
class IssueCollector {
 public:
  // Args:
  //  severity: inclusive limit for issues to return as non-ok absl::Status.
  explicit IssueCollector(RuntimeIssue::Severity severity_limit)
      : severity_limit_(severity_limit) {}

  // move-only.
  IssueCollector(const IssueCollector&) = delete;
  IssueCollector& operator=(const IssueCollector&) = delete;
  IssueCollector(IssueCollector&&) = default;
  IssueCollector& operator=(IssueCollector&&) = default;

  // Collect an Issue.
  // Returns a status according to the IssueCollector's policy and the given
  // Issue.
  // The Issue is always added to issues, regardless of whether AddIssue returns
  // a non-ok status.
  absl::Status AddIssue(RuntimeIssue issue) {
    issues_.push_back(std::move(issue));
    if (issues_.back().severity() >= severity_limit_) {
      return issues_.back().ToStatus();
    }
    return absl::OkStatus();
  }

  absl::Span<const RuntimeIssue> issues() const { return issues_; }
  std::vector<RuntimeIssue> ExtractIssues() { return std::move(issues_); }

 private:
  RuntimeIssue::Severity severity_limit_;
  std::vector<RuntimeIssue> issues_;
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_ISSUE_COLLECTOR_H_
